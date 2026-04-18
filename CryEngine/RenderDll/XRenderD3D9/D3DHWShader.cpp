/*=============================================================================
D3DHWShader.cpp : D3D specific shaders support.
Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"
#include "IDirectBee.h"
#include <crc32.h>

#if defined(OPENGL)
SGlobalConstMap CHWShader_D3D::sGlobalConsts[CHWShader_D3D::scSGlobalConstMapCount];
#endif
//=======================================================================

DEFINE_ALIGNED_DATA(Vec4, CHWShader_D3D::m_CurPSParams[224], 16);
DEFINE_ALIGNED_DATA(Vec4, CHWShader_D3D::m_CurVSParams[256], 16);
#if defined (DIRECT3D9) || defined(OPENGL)
DEFINE_ALIGNED_DATA(Vec4, CHWShader_D3D::m_CurPSParamsI[16], 16);
DEFINE_ALIGNED_DATA(Vec4, CHWShader_D3D::m_CurVSParamsI[16], 16);
#elif defined (DIRECT3D10)
ID3D10Buffer **CHWShader_D3D::m_pCB[eHWSC_Max][CB_MAX];
void **CHWShader_D3D::m_pCBShadow[eHWSC_Max][CB_MAX];
ID3D10Buffer *CHWShader_D3D::m_pCurReqCB[eHWSC_Max][CB_MAX];
ID3D10Buffer *CHWShader_D3D::m_pCurDevCB[eHWSC_Max][CB_MAX];
float *CHWShader_D3D::m_pDataCB[eHWSC_Max][CB_MAX];
int CHWShader_D3D::m_nCurMaxVecs[eHWSC_Max][CB_MAX];
int CHWShader_D3D::m_nMax_PF_Vecs[eHWSC_Max];
int CHWShader_D3D::m_nMax_SG_Vecs[eHWSC_Max];
ID3D10Buffer *CHWShader_D3D::m_pLightCB[eHWSC_Max];
CHWShader_D3D::SHWSInstance *CHWShader_D3D::m_pCurInstVS;
CHWShader_D3D::SHWSInstance *CHWShader_D3D::m_pCurInstPS;
CHWShader_D3D::SHWSInstance *CHWShader_D3D::m_pCurInstGS;
#endif

std::vector<SCGParam> CHWShader_D3D::m_PF_Params[eHWSC_Max];
std::vector<SCGParam> CHWShader_D3D::m_SG_Params[eHWSC_Max];
std::vector<SCGParam> CHWShader_D3D::m_CM_Params[eHWSC_Max];

CHWShader_D3D::InstanceMap CHWShader_D3D::m_SharedInsts;

volatile LONG SShaderAsyncInfo::s_nPendingAsyncShaders = 0;

DynArray<SShaderTechniqueStat> g_SelectedTechs;

int CHWShader_D3D::m_FrameObj;

bool CHWShader_D3D::ms_bInitShaders = true;

int CHWShader_D3D::m_PSParamsToCommit[256];
int CHWShader_D3D::m_NumPSParamsToCommit;
int CHWShader_D3D::m_VSParamsToCommit[256];
int CHWShader_D3D::m_NumVSParamsToCommit;

int CHWShader_D3D::m_nResetDeviceFrame = -1;
int CHWShader_D3D::m_nInstFrame = -1;

SD3DShader *CHWShader::m_pCurPS;
SD3DShader *CHWShader::m_pCurVS;
SD3DShader *CHWShader::m_pCurGS;

FXShaderCache CHWShader::m_ShaderCache;
FXShaderCacheNames CHWShader::m_ShaderCacheList;

#if defined(PS3) && !defined(CRY_USE_GCM)
// FIXME
// Temporary workaround for a bug/limitation in the Cell SDK PSGL
// library.
void *GetShaderProgram(CGprogram);
void *GetShaderProgram(CGprogram, size_t&);
void ClearShaderProgram(CGprogram);

extern char *const ps3ShaderName;
extern const size_t ps3ShaderName_size;
#endif

extern float fTime0;
extern float fTime1;
extern float fTime2;

void CHWShader_D3D::SHWSInstance::Release(SShaderCache *pCache)
{
  EHWShaderClass eSHClass;
  switch (m_eProfileType)
  {
    case eHWSP_VS_1_1:
    case eHWSP_VS_2_0:
    case eHWSP_VS_3_0:
    case eHWSP_VS_4_0:
    case eHWSP_VS_Auto:
      eSHClass = eHWSC_Vertex;
      break;
    case eHWSP_PS_1_1:
    case eHWSP_PS_2_0:
    case eHWSP_PS_2_X:
    case eHWSP_PS_3_0:
    case eHWSP_PS_4_0:
    case eHWSP_PS_Auto:
      eSHClass = eHWSC_Pixel;
      break;
    case eHWSP_GS_4_0:
      eSHClass = eHWSC_Geometry;
      break;
    default:
      assert(0);
      break;
  }

  SAFE_DELETE(m_pSamplers);
  SAFE_DELETE(m_pBindVars);
  SAFE_DELETE(m_pParams[0]);
  SAFE_DELETE(m_pParams[1]);
  SAFE_DELETE(m_pParams_Inst);
  int nCount = -1;
  if (m_Handle.m_pShader)
  {
    if (eSHClass == eHWSC_Pixel)
    {
      SD3DShader *pPS = m_Handle.m_pShader;
      if (pPS)
      {
        nCount = m_Handle.Release(eSHClass);
        if (!nCount && CHWShader::m_pCurPS == pPS)
          CHWShader::m_pCurPS = NULL;
      }
    }
    else
    if (eSHClass == eHWSC_Vertex)
    {
      SD3DShader *pVS = m_Handle.m_pShader;
      if (pVS)
      {
        nCount = m_Handle.Release(eSHClass);
        if (!nCount && CHWShader::m_pCurVS == pVS)
          CHWShader::m_pCurVS = NULL;
      }
    }
    else
    if (eSHClass == eHWSC_Geometry)
    {
      SD3DShader *pGS = m_Handle.m_pShader;
      if (pGS)
      {
        nCount = m_Handle.Release(eSHClass);
        if (!nCount && CHWShader::m_pCurGS == pGS)
          CHWShader::m_pCurGS = NULL;
      }
    }
  }
#ifdef DIRECT3D10
  SAFE_DELETE_VOID_ARRAY(m_pShaderData);
#endif

  if (!nCount && pCache && !pCache->m_DeviceShaders.empty())
    pCache->m_DeviceShaders.erase(m_DeviceObjectID);
  m_Handle.m_pShader = NULL;
}


CHWShader_D3D::SHWSSharedList::~SHWSSharedList()
{
  int i, j;
  for (i=0; i<m_SharedInsts.Num(); i++)
  {
    SHWSSharedInstance *pSInst = &m_SharedInsts[i];
    for (j=0; j<pSInst->m_Insts.Num(); j++)
    {
      SHWSInstance *pInst = &pSInst->m_Insts[j];
      pInst->Release();
    }
    pSInst->m_Insts.Free();
  }
}

void CHWShader_D3D::ShutDown()
{
  int i;
  for (i=0; i<eHWSC_Max; i++)
  {
    m_PF_Params[i].clear();
    m_SG_Params[i].clear();
  }

  InstanceMapItor it;
  for (it=m_SharedInsts.begin(); it!=m_SharedInsts.end(); it++)
  {
    SHWSSharedList *pL = it->second;
    SAFE_DELETE(pL);
  }
  m_SharedInsts.clear();

  while (!m_ShaderCache.empty())
  {
    SShaderCache *pC = m_ShaderCache.begin()->second;
    SAFE_RELEASE(pC);
  }
  m_ShaderCacheList.clear();
  g_SelectedTechs.clear();
}

CHWShader *CHWShader::mfForName(const char *name, const char *nameSource, uint32 CRC32, DynArray<STexSampler>& Samplers, DynArray<SFXParam>& Params, uint32 dwEntryFunc, EHWShaderClass eClass, EHWSProfile eSHV, DynArray<uint32>& SHData, FXShaderToken& Table, uint32 dwType, uint nMaskGen, uint nMaskGenFX)
{
  if (!name || !name[0])
    return NULL;

  CHWShader_D3D *pSH = NULL;
  string strName = name;
  CCryName className = mfGetClassName(eClass);
  string AddStr;

  if (CParserBin::m_bD3D10)
  {
    if (eSHV < eHWSP_PS_1_1)
      eSHV = eHWSP_VS_4_0;
    else
    if (eSHV < eHWSP_GS_4_0)
      eSHV = eHWSP_PS_4_0;
  }

  if (nMaskGen)
  {
    strName += AddStr.Format("(%x)", nMaskGen);
  }
  if (eSHV != eHWSP_PS_Auto && eSHV != eHWSP_VS_Auto)
  {
    const char *pProf = mfProfileToString(eSHV);
    if (pProf)
      strName += AddStr.Format("(%s)", pProf);
  }
  CCryName Name = strName.c_str();
  CBaseResource *pBR = CBaseResource::GetResource(className, Name, false);
  if (!pBR)
  {
    pSH = new CHWShader_D3D;
    pSH->m_NameSourceFX = nameSource;
    pSH->Register(className, Name);
    pSH->m_EntryFunc = CParserBin::GetString(dwEntryFunc, Table);
    pSH->mfFree(CRC32);
  }
  else
  {
    pSH = (CHWShader_D3D *)pBR;
    pSH->AddRef();
    if (pSH->m_CRC32 == CRC32)
      return pSH;
    pSH->mfFree(CRC32);
    pSH->m_CRC32 = CRC32;
  }

  pSH->m_dwShaderType = dwType;
  pSH->m_eSHClass = eClass;
  pSH->m_nMaskGenShader = nMaskGen;
  pSH->m_nMaskGenFX = nMaskGenFX;
  pSH->m_Table = Table;
  pSH->m_TokenData = SHData;
  pSH->m_Samplers = Samplers;
  pSH->m_Params = Params;
  pSH->m_CRC32 = CRC32;

  if (eSHV == eHWSP_PS_4_0 || eSHV == eHWSP_VS_4_0 || eSHV == eHWSP_GS_4_0)
    pSH->m_Flags |= HWSG_40ONLY;
  else
  if (eSHV == eHWSP_PS_2_0 || eSHV == eHWSP_VS_2_0)
    pSH->m_Flags |= HWSG_20ONLY;
  else
  if (eSHV == eHWSP_PS_3_0 || eSHV == eHWSP_VS_3_0)
    pSH->m_Flags |= HWSG_30ONLY;
  else
  if (eSHV == eHWSP_PS_2_X)
    pSH->m_Flags |= HWSG_2XONLY;
  else
  if (eSHV == eHWSP_PS_Auto || eSHV == eHWSP_VS_Auto)
    pSH->m_Flags |= HWSG_AUTOENUMTC;
  else
  if (eSHV != eHWSP_VS_1_1 && eSHV != eHWSP_PS_1_1)
    assert(0);

  pSH->mfConstructFX();

  return pSH;
}


void CHWShader_D3D::SetTokenFlags(uint32 nToken)
{
  switch (nToken)
  {
  case eT__LT_LIGHTS:
    m_Flags |= HWSG_SUPPORTS_LIGHTING;
    break;
  case eT__LT_0_TYPE:
  case eT__LT_1_TYPE:
  case eT__LT_2_TYPE:
  case eT__LT_3_TYPE:
    m_Flags |= HWSG_SUPPORTS_MULTILIGHTS;
    break;
  case eT__TT0_TCM:
  case eT__TT1_TCM:
  case eT__TT2_TCM:
  case eT__TT3_TCM:
  case eT__TT0_TCG_TYPE:
  case eT__TT1_TCG_TYPE:
  case eT__TT2_TCG_TYPE:
  case eT__TT3_TCG_TYPE:
  case eT__TT0_TCPROJ:
  case eT__TT1_TCPROJ:
  case eT__TT2_TCPROJ:
  case eT__TT3_TCPROJ:
  case eT__TT0_TCUBE:
  case eT__TT1_TCUBE:
  case eT__TT2_TCUBE:
  case eT__TT3_TCUBE:
    m_Flags |= HWSG_SUPPORTS_MODIF;
    break;
  case eT__VT_TYPE:
    m_Flags |= HWSG_SUPPORTS_VMODIF;
    break;
  case eT__FT_TEXTURE:
    m_Flags |= HWSG_FP_EMULATION;
    break;
  }
}

uint64 CHWShader_D3D::CheckToken(uint32 nToken)
{
  uint64 nMask = 0;
  SShaderGen *pGen = gRenDev->m_cEF.m_pGlobalExt;
  int i;
  for (i=0; i<pGen->m_BitMask.Num(); i++)
  {
    SShaderGenBit *bit = pGen->m_BitMask[i];
    if (!bit)
      continue;

    if (bit->m_dwToken == nToken)
    {
      nMask |= bit->m_Mask;
      break;
    }
  }
  if (!nMask)
    SetTokenFlags(nToken);

  return nMask;
}

uint64 CHWShader_D3D::CheckIfExpr_r(uint32 *pTokens, uint32& nCur, uint32 nSize)
{
  uint64 nMask = 0;

  while (nCur < nSize)
  {
    int nRecurs = 0;
    uint32 nToken = pTokens[nCur++];
    if (nToken == eT_br_rnd_1) // check for '('
    {
      uint32 tmpBuf[64];
      int n = 0;
      int nD = 0;
      while (true)
      {
        nToken = pTokens[nCur];
        if (nToken == eT_br_rnd_1) // check for '('
          n++;
        else
        if (nToken == eT_br_rnd_2) // check for ')'
        {
          if (!n)
          {
            tmpBuf[nD] = 0;
            nCur++;
            break;
          }
          n--;
        }
        else
        if (nToken == 0)
          return nMask;
        tmpBuf[nD++] = nToken;
        nCur++;
      }
      if (nD)
      {
        uint32 nC = 0;
        nMask |= CheckIfExpr_r(tmpBuf, nC, nSize);
      }
    }
    else
    {
      bool bNeg = false;
      if (nToken == eT_excl)
      {
        bNeg = true;
        nToken = pTokens[nCur++];
      }
      nMask |= CheckToken(nToken);
    }
    nToken = pTokens[nCur];
    if (nToken == eT_or)
    {
      nCur++;
      assert (pTokens[nCur] == eT_or);
      if (pTokens[nCur] == eT_or)
        nCur++;
    }
    else
    if (nToken == eT_and)
    {
      nCur++;
      assert (pTokens[nCur] == eT_and);
      if (pTokens[nCur] == eT_and)
        nCur++;
    }
    else
      break;
  }
  return nMask;
}

uint64 CHWShader_D3D::mfConstructFX_AffectMask_RT()
{
  assert(gRenDev->m_cEF.m_pGlobalExt);
  if (!gRenDev->m_cEF.m_pGlobalExt)
    return 0;
  uint64 nMask = 0;
  SShaderGen *pGen = gRenDev->m_cEF.m_pGlobalExt;

  assert(!m_TokenData.empty());
  uint32 *pTokens = &m_TokenData[0];
  int nSize = m_TokenData.size();
  uint32 nCur = 0;
  while (nCur < nSize)
  {
    uint32 nTok = CParserBin::NextToken(pTokens, nCur, nSize-1);
    if (!nTok)
      continue;
    if (nTok >= eT_if && nTok <= eT_elif)
      nMask |= CheckIfExpr_r(pTokens, nCur, nSize);
    else
      SetTokenFlags(nTok);
  }
  // Reset any RT bits for this shader if this shader type is not existing for specific bit
  // See Runtime.ext file
  if (m_dwShaderType)
  {
    for (int i=0; i<pGen->m_BitMask.Num(); i++)
    {
      SShaderGenBit *bit = pGen->m_BitMask[i];
      if (!bit)
        continue;
      int j;
      if (bit->m_PrecacheNames.size())
      {
        for (j=0; j<bit->m_PrecacheNames.size(); j++)
        {
          if (m_dwShaderType == bit->m_PrecacheNames[j])
            break;
        }
        if (j == bit->m_PrecacheNames.size())
          nMask &= ~bit->m_Mask;
      }
    }
  }
  return nMask;
}

void CHWShader_D3D::SHWSInstance::CopyFrom (CHWShader_D3D::SHWSInstance *pInst, EHWShaderClass Class, SShaderCache *pCache)
{
  m_Handle = pInst->m_Handle;
  m_Handle.AddRef();
  assert (pInst->m_DeviceObjectID < SH_UNIQ_ID);
  if (pInst->m_DeviceObjectID < SH_UNIQ_ID && pInst->m_Handle.m_pShader)
  {
    FXDeviceShaderItor it = pCache->m_DeviceShaders.find(pInst->m_DeviceObjectID);
    if (it == pCache->m_DeviceShaders.end())
    {
      pCache->m_DeviceShaders.insert(FXDeviceShaderItor::value_type(pInst->m_DeviceObjectID, pInst->m_Handle.m_pShader));
    }
  }

  m_eProfileType = pInst->m_eProfileType;
  m_nInstructions = pInst->m_nInstructions;
  m_nInstIndex = pInst->m_nInstIndex;
  m_VStreamMask_Decl = pInst->m_VStreamMask_Decl;
  m_VStreamMask_Stream = pInst->m_VStreamMask_Stream;
  m_bShared = pInst->m_bShared;
  m_bHasPMParams = pInst->m_bHasPMParams;
  m_nVertexFormat = pInst->m_nVertexFormat;
  m_nNumInstAttributes = pInst->m_nNumInstAttributes;
  m_DeviceObjectID = pInst->m_DeviceObjectID;
  if (pInst->m_pParams[0] && pInst->m_pParams[0]->size())
  {
    m_pParams[0] = new std::vector<SCGParam>;
    *m_pParams[0] = *pInst->m_pParams[0];
  }
  if (pInst->m_pParams[1] && pInst->m_pParams[1]->size())
  {
    m_pParams[1] = new std::vector<SCGParam>;
    *m_pParams[1] = *pInst->m_pParams[1];
  }
  if (pInst->m_pSamplers && pInst->m_pSamplers->size())
  {
    m_pSamplers = new DynArray<STexSampler>;
    *m_pSamplers = *pInst->m_pSamplers;
  }
  if (pInst->m_pBindVars && pInst->m_pBindVars->size())
  {
    m_pBindVars = new std::vector<SCGBind>;
    *m_pBindVars = *pInst->m_pBindVars;
  }
  if (pInst->m_pParams_Inst && pInst->m_pParams_Inst->size())
  {
    m_pParams_Inst = new std::vector<SCGParam>;
    *m_pParams_Inst = *pInst->m_pParams_Inst;
  }
#if defined (DIRECT3D10)
  m_nMaxVecs[0] = pInst->m_nMaxVecs[0];
  m_nMaxVecs[1] = pInst->m_nMaxVecs[1];
#endif
}

void CHWShader_D3D::mfConstructFX()
{
  if (!strnicmp(m_EntryFunc.c_str(), "Common_", 7))
    m_Flags |= HWSG_SHARED;
  if (!strnicmp(m_EntryFunc.c_str(), "Sync_", 5))
    m_Flags |= HWSG_SYNC;

  m_nMaskAffect_RT = mfConstructFX_AffectMask_RT();

  //if (!gRenDev->m_cEF.m_bReload)
  int nAsync = CRenderer::CV_r_shadersasynccompiling;
  CRenderer::CV_r_shadersasynccompiling = 0;
  mfPrecache();
  CRenderer::CV_r_shadersasynccompiling = nAsync;
}

void CHWShader_D3D::mfAddFXParameter(SHWSInstance *pInst, DynArray<SFXParam>& Params, DynArray<STexSampler>& Samplers, SFXParam *pr, const char *ParamName, SCGBind *pBind, CShader *ef, bool bInstParam, EHWShaderClass eSHClass)
{
  SCGParam CGpr;

  assert(pBind);
  if (!pBind)
    return;

  int nComps = 0;
  int nParams = pBind->m_nParameters;
  if (pr->m_Assign.size())
    nComps = pr->m_nComps;
  else
  {
    for (int i=0; i<pr->m_nComps; i++)
    {
      string cur = pr->GetParamComp(i);
      if (!cur[0])
        break;
      nComps++;
    }
  }
  // Process parameters only with semantics
  if (nComps && nParams)
  {
    std::vector<SCGParam> *pParams;
    if (pr->m_nParameters > 1)
    {
      if (!bInstParam)
      {
        if (!pInst->m_pParams[0])
          pInst->m_pParams[0] = new std::vector<SCGParam>;
        pParams = pInst->m_pParams[0];
      }
      else
      {
        if (!pInst->m_pParams_Inst)
          pInst->m_pParams_Inst = new std::vector<SCGParam>;
        pParams = pInst->m_pParams_Inst;
      }
    }
    else
      if (bInstParam)
      {
        if (!pInst->m_pParams_Inst)
          pInst->m_pParams_Inst = new std::vector<SCGParam>;
        pParams = pInst->m_pParams_Inst;
      }
      else
      {
        if (!pInst->m_pParams[0])
          pInst->m_pParams[0] = new std::vector<SCGParam>;
        pParams = pInst->m_pParams[0];
      }
      uint nOffs = pParams->size();
      bool bRes = gRenDev->m_cEF.mfParseFXParameter(Params, pr, &Samplers, ParamName, ef, bInstParam, pBind->m_nParameters, pParams, eSHClass, false);
      assert(bRes);
      if (pParams->size() > nOffs)
      {
        for (int i=0; i<pParams->size()-nOffs; i++)
        {
          //assert(pBind->m_nComponents == 1);
          SCGParam &p = (*pParams)[nOffs+i];
          p.m_dwBind = pBind->m_dwBind+i;
#if defined(DIRECT3D10)
          p.m_dwCBufSlot = pBind->m_dwCBufSlot;
#endif
#if defined(OPENGL)
          p.m_isMatrix = pBind->m_isMatrix;
#endif
        }
      }
  }
  // Parameter without semantic
}

struct SAliasSampler
{
  STexSampler *fxSampler;
  string NameINT;
  SAliasSampler()
  {
    fxSampler = NULL;
  }
};

bool CHWShader_D3D::mfAddFXParameter(SHWSInstance *pInst, DynArray<SFXParam>& Params, DynArray<STexSampler>& Samplers, const char *param, const char *paramINT, SCGBind *bn, bool bInstParam, EHWShaderClass eSHClass)
{
  SFXParam *pr = gRenDev->m_cEF.mfGetFXParameter(Params, param);
  if (pr)
  {
    if (bn->m_nParameters < 0)
      bn->m_nParameters = pr->m_nParameters;
    mfAddFXParameter(pInst, Params, Samplers, pr, paramINT, bn, NULL, bInstParam, eSHClass);
    return true;
  }
  return false;
}

//==================================================================================================================

int CGParamCallback( const VOID* arg1, const VOID* arg2 )
{
  SCGParam *pi1 = (SCGParam *)arg1;
  SCGParam *pi2 = (SCGParam *)arg2;
  if (pi1->m_dwBind < pi2->m_dwBind)
    return -1;
  if (pi1->m_dwBind > pi2->m_dwBind)
    return 1;
  return 0;
}

char *szNamesCB[CB_MAX] = {"PER_BATCH", "PER_INSTANCE", "PER_FRAME", "PER_MATERIAL", "PER_LIGHT", "PER_SHADOWGEN", "SKIN_DATA", "SHAPE_DATA", "INSTANCE_DATA"};
void CHWShader_D3D::mfCreateBinds(SHWSInstance *pInst, LPD3DXCONSTANTTABLE pConstantTable, byte* pShader, int nSize)
{
#if defined (DIRECT3D9) || defined(OPENGL)
  D3DXCONSTANTTABLE_DESC CTDesc;
  pConstantTable->GetDesc(&CTDesc);
  for (uint i=0; i<CTDesc.Constants; i++)
  {
    D3DXCONSTANT_DESC CDesc;
    uint nCount = 1;
    D3DXHANDLE cHandle = pConstantTable->GetConstant(NULL, i);
    pConstantTable->GetConstantDesc(cHandle, &CDesc, &nCount);
    if (CDesc.RegisterSet == D3DXRS_SAMPLER)
    {
      SCGBind cgp;
      if (!pInst->m_pBindVars)
        pInst->m_pBindVars = new std::vector<SCGBind>;
      cgp.m_dwBind = CDesc.RegisterIndex | SHADER_BIND_SAMPLER;
      cgp.m_Flags = CParserBin::GetCRC32(CDesc.Name);
      cgp.m_nParameters = CDesc.RegisterCount;
      cgp.m_Name = CDesc.Name;
      pInst->m_pBindVars->push_back(cgp);
    }
    else
    if (CDesc.RegisterSet == D3DXRS_FLOAT4 || CDesc.RegisterSet == D3DXRS_INT4 || CDesc.RegisterSet == D3DXRS_BOOL)
    {
      SCGBind cgp;
      cgp.m_dwBind = CDesc.RegisterIndex;
#if defined(OPENGL)
      assert(cgp.m_dwBind < ~scParamMask);//otherwise we have a handle overwriting the bits
      //mark as matrix which important later on where we set the constants
      switch(CDesc.Class)
      {
      case D3DXPC_MATRIX_ROWS2:
        cgp.m_isMatrix = scIs2x4Matrix;
        break;
      case D3DXPC_MATRIX_ROWS3:
        cgp.m_isMatrix = scIs3x4Matrix;
        break;
      case D3DXPC_MATRIX_ROWS4:
        cgp.m_isMatrix = scIs4x4Matrix;
        break;
      }
#endif
      cgp.m_nParameters = CDesc.RegisterCount;
      cgp.m_Name = CDesc.Name;
      cgp.m_Flags = CParserBin::GetCRC32(CDesc.Name);
      if (!pInst->m_pBindVars)
        pInst->m_pBindVars = new std::vector<SCGBind>;
      pInst->m_pBindVars->push_back(cgp);
    }
    else
    {
      assert(false);
    }
  }
#elif defined (DIRECT3D10)
  uint i;
  ID3D10ShaderReflection *pShaderReflection = (ID3D10ShaderReflection *)pConstantTable;
  D3D10_SHADER_DESC Desc;
  pShaderReflection->GetDesc(&Desc);
  ID3D10ShaderReflectionConstantBuffer* pCB = NULL;
  for (int n=0; n<Desc.ConstantBuffers; n++)
  {
    pCB = pShaderReflection->GetConstantBufferByIndex(n);
    D3D10_SHADER_BUFFER_DESC SBDesc;
    pCB->GetDesc(&SBDesc);
#if !defined(PS3)
    int nCB;
    if (!strcmp("$Globals", SBDesc.Name))
      nCB = CB_PER_BATCH;
    else
    for (nCB=0; nCB<CB_MAX; nCB++)
    {
      if (!strcmp(szNamesCB[nCB], SBDesc.Name))
        break;
    }
    assert(nCB != CB_MAX);
    if (nCB == CB_MAX)
      continue;
#endif
    for (i=0; i<SBDesc.Variables; i++)
    {
      uint nCount = 1;
      ID3D10ShaderReflectionVariable* pCV = pCB->GetVariableByIndex(i);
      ID3D10ShaderReflectionType* pVT = pCV->GetType();
      D3D10_SHADER_VARIABLE_DESC CDesc;
      D3D10_SHADER_TYPE_DESC CTDesc;
      pVT->GetDesc(&CTDesc);
      pCV->GetDesc(&CDesc);
      if (!(CDesc.uFlags & D3D10_SVF_USED))
        continue;
      if (CTDesc.Class==D3D10_SVC_VECTOR || CTDesc.Class==D3D10_SVC_SCALAR || CTDesc.Class==D3D10_SVC_MATRIX_COLUMNS || CTDesc.Class==D3D10_SVC_MATRIX_ROWS)
      {
        SCGBind cgp;
        //assert(!(CDesc.StartOffset & 0xf));
        int nReg = CDesc.StartOffset>>2;
        cgp.m_dwBind = nReg; //<<2;
#if defined(PS3)
        cgp.m_dwCBufSlot = CDesc.CBufferIndex;
#else
        cgp.m_dwCBufSlot = nCB;
#endif
        cgp.m_nParameters = CDesc.Size>>2;
        cgp.m_Name = CDesc.Name;
        cgp.m_Flags = CParserBin::GetCRC32(CDesc.Name);
        if (!pInst->m_pBindVars)
          pInst->m_pBindVars = new std::vector<SCGBind>;
        pInst->m_pBindVars->push_back(cgp);
      }
      else
      {
        assert(false);
      }
    }
  }
  D3D10_SHADER_INPUT_BIND_DESC IBDesc;
  for (i=0; i<Desc.BoundResources; i++)
  {
    ZeroStruct(IBDesc);
    pShaderReflection->GetResourceBindingDesc(i, &IBDesc);
    if (IBDesc.Type != D3D10_SIT_TEXTURE)
      continue;
    SCGBind cgp;
    if (!pInst->m_pBindVars)
      pInst->m_pBindVars = new std::vector<SCGBind>;
#if defined(PS3)
		cgp.m_dwCBufSlot	=	IBDesc.BindPoint;
#endif

    cgp.m_dwBind = IBDesc.BindPoint | SHADER_BIND_SAMPLER;

    cgp.m_nParameters = IBDesc.BindCount;
    cgp.m_Name = IBDesc.Name;
    cgp.m_Flags = CParserBin::GetCRC32(IBDesc.Name);
    pInst->m_pBindVars->push_back(cgp);
  }
#if !defined(PS3)
  if (pInst->m_pBindVars)
  {
    for (i=0; i<pInst->m_pBindVars->size(); i++)
    {
      SCGBind *pB = &(*pInst->m_pBindVars)[i];
      if (!(pB->m_dwBind & SHADER_BIND_SAMPLER))
        continue;
      int j;
      for (j=0; j<Desc.BoundResources; j++)
      {
        ZeroStruct(IBDesc);
        pShaderReflection->GetResourceBindingDesc(j, &IBDesc);

        if (IBDesc.Type != D3D10_SIT_SAMPLER )
          continue;

        if (!stricmp(IBDesc.Name, pB->m_Name.c_str()) )
        {
          pB->m_dwCBufSlot = IBDesc.BindPoint;
          break;
        }

        if (!strnicmp(IBDesc.Name, "SAMPLER_STATE_", 14))
        {
          if(strstr(pB->m_Name.c_str(), &(IBDesc.Name[15]))!=NULL)
          {
            pB->m_dwCBufSlot = IBDesc.BindPoint;
            break;
          }
        }

      }
      if (j == Desc.BoundResources /*&& strnicmp(pB->m_Name.c_str(), "sceneDepthSamplerMS", 19)!=0*/)
      {
        //assert(0);
      }
    }
  }
#endif
  /*if (pInst->m_pBindVars)
  {
  TArray<int> Regs[CB_MAX];
  int nCBs[CB_MAX];
  memset(nCBs, 0, sizeof(nCBs));lau
  for (i=0; i<pInst->m_pBindVars->size(); i++)
  {
  SCGBind *pB = &(*pInst->m_pBindVars)[i];
  if (pB->m_dwBind & SHADER_BIND_SAMPLER)
  continue;
  assert(pB->m_dwCBuf==CB_PER_INSTANCE || pB->m_dwCBuf==CB_PER_BATCH);
  int nReg = pB->m_dwBind>>2;
  for (j=0; j<Regs[pB->m_dwCBuf].Num(); j++)
  {
  if (nReg == Regs[pB->m_dwCBuf][j])
  break;
  }
  if (j == Regs[pB->m_dwCBuf].Num())
  {
  Regs[pB->m_dwCBuf].AddElem(nReg);
  int nFloats = (pB->m_nParameters+3) & ~3;
  nCBs[pB->m_dwCBuf] += nFloats;
  }
  }
  for (i=0; i<CB_MAX; i++)
  {
  if (!nCBs[i])
  continue;
  uint nSize = nCBs[i] * sizeof(float);
  D3D10_BUFFER_DESC bd;
  ZeroStruct(bd);
  bd.Usage = D3D10_USAGE_DYNAMIC;
  bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  bd.MiscFlags = 0;

  //bd.ByteWidth = nSize;

  D3D10_SHADER_BUFFER_DESC SBDesc;
  pShaderReflection->GetConstantBufferByIndex(i);
  pCB->GetDesc(&SBDesc);
  bd.ByteWidth = SBDesc.Size;

  HRESULT hr = gcpRendD3D->GetD3DDevice()->CreateBuffer(&bd, NULL, &pInst->m_pCB[i==CB_PER_BATCH ? 0 : 1]);
  assert(SUCCEEDED(hr));
  }
  }*/
#endif
}

void CHWShader_D3D::mfGatherFXParameters(SHWSInstance *pInst, std::vector<SCGBind>* BindVars, DynArray<SCGBind> *InstBindVars, CHWShader_D3D *pSH, int nFlags)
{
  uint i, j;
  SAliasSampler samps[MAX_TMU];
  int nMaxSampler = -1;
  int nParam = 0;
  if (pInst->m_pBindVars)
  {
    for (i=0; i<pInst->m_pBindVars->size(); i++)
    {
      SCGBind *bn = &(*BindVars)[i];
      const char *param = bn->m_Name.c_str();
      if (!strncmp(param, "_g_", 3))
        continue;
      const char *paramINT = param;
      bool bSampler = (bn->m_dwBind & SHADER_BIND_SAMPLER) != 0;
      if (!bSampler)
      {
        if (nFlags < 2)
        {
          bool bRes = mfAddFXParameter(pInst, pSH->m_Params, pSH->m_Samplers, param, paramINT, bn, false, pSH->m_eSHClass);
          if (!bRes)
          {
            iLog->LogWarning("WARNING: Couldn't find parameter '%s' for shader '%s'", param, pSH->GetName());
            // const parameters aren't listed in Params
            // assert(0);
          }
        }
      }
      else
      {
        for (j=0; j<pSH->m_Samplers.size(); j++)
        {
          STexSampler *sm = &pSH->m_Samplers[j];
          if (!stricmp(sm->m_Name.c_str(), param))
          {
            int nSampler = bn->m_dwBind & 0xf;
            nMaxSampler = max(nSampler, nMaxSampler);
            samps[nSampler].fxSampler = sm;
#if defined (DIRECT3D10)
            sm->m_nSamplerSlot = bn->m_dwCBufSlot;
#else
            sm->m_nSamplerSlot = bn->m_dwBind;
#endif
            samps[nSampler].NameINT = paramINT;
            break;
          }
        }
        if (j == pSH->m_Samplers.size())
        {
          for (j=0; j<pSH->m_Samplers.size(); j++)
          {
            STexSampler *sm = &pSH->m_Samplers[j];
            const char *src = sm->m_Name.c_str();
            char name[128];
            int n = 0;
            while(src[n])
            {
              if (src[n] <= 0x20 || src[n] == '[')
                break;
              name[n] = src[n];
              n++;
            }
            name[n] = 0;
            if (!stricmp(name, param))
            {
              int nSampler = bn->m_dwBind & 0xf;
#if defined (DIRECT3D10)
              sm->m_nSamplerSlot = bn->m_dwCBufSlot;
#endif
              for (int nS=0; nS<bn->m_nParameters; nS++)
              {
                nMaxSampler = max(nSampler+nS, nMaxSampler);
                samps[nSampler+nS].fxSampler = sm;
                samps[nSampler+nS].NameINT = paramINT;
              }
              break;
            }
          }
          if (j == pSH->m_Samplers.size())
          {
            assert(0);
          }
        }
      }
    }
  }
  if (nFlags != 1)
  {
    for (i=0; (int)i<=nMaxSampler; i++)
    {
      STexSampler *smp = samps[i].fxSampler;
      if (!smp)
        continue;
      CTexture *tp = gRenDev->m_cEF.mfParseFXTechnique_LoadShaderTexture(smp, NULL, NULL, i, eCO_NOSET, eCO_NOSET, DEF_TEXARG0, DEF_TEXARG0);
      smp->m_pTex = tp;
      if (!pInst->m_pSamplers)
        pInst->m_pSamplers = new DynArray<STexSampler>;
      pInst->m_pSamplers->push_back(*smp);
    }
  }
  else
  {
#if !defined(XENON) && !defined(PS3)
    assert(pInst->m_pAsync);
    if (pInst->m_pAsync && nMaxSampler >= 0)
      pInst->m_pAsync->m_bPendedSamplers = true;
#endif
  }

#if defined (DIRECT3D10)
  pInst->m_nMaxVecs[0] = pInst->m_nMaxVecs[1] = 0;
  if (pInst->m_pBindVars)
  {
    for (i=0; i<pInst->m_pBindVars->size(); i++)
    {
      SCGBind *pB = &(*pInst->m_pBindVars)[i];
      if (pB->m_dwBind & SHADER_BIND_SAMPLER)
        continue;
      if (pInst->m_pParams[0])
      {
        for (j=0; j<pInst->m_pParams[0]->size(); j++)
        {
          SCGParam *pr = &(*pInst->m_pParams[0])[j];
          if (pr->m_dwBind == pB->m_dwBind && pr->m_Name == pB->m_Name)
            break;
        }
        if (j != pInst->m_pParams[0]->size())
          continue;
      }
      //cgi->m_nMaxVecs[CB_PER_BATCH] = max((pB->m_dwBind>>2)+((pB->m_nParameters+3)>>2), cgi->m_nMaxVecs[CB_PER_BATCH]);
      if(pB->m_dwCBufSlot<3)
				pInst->m_nMaxVecs[pB->m_dwCBufSlot] = max(((pB->m_dwBind + pB->m_nParameters + 3) >> 2), pInst->m_nMaxVecs[pB->m_dwCBufSlot]);
    }
  }
#endif
  if (pInst->m_pParams[0])
  {
    for (i=0; i<pInst->m_pParams[0]->size(); i++)
    {
      SCGParam *pr = &(*pInst->m_pParams[0])[i];

      if (pr->m_Flags & PF_MATERIAL)
        pInst->m_bHasPMParams = true;
    }

    gRenDev->m_cEF.mfCheckObjectDependParams(pInst->m_pParams[0], pInst->m_pParams[1], pSH->m_eSHClass);
  }

#if defined (DIRECT3D10)
  for (i=0; i<2; i++)
  {
    if (pInst->m_pParams[i])
    {
      for (j=0; j<pInst->m_pParams[i]->size(); j++)
      {
        SCGParam *pr = &(*pInst->m_pParams[i])[j];
        //cgi->m_nMaxVecs[i] = max((pr->m_dwBind>>2)+((pr->m_nParameters+3)>>2), cgi->m_nMaxVecs[i]);
				pInst->m_nMaxVecs[i] = max(((pr->m_dwBind + pr->m_nParameters + 3) >> 2), pInst->m_nMaxVecs[i]);
      }
    }
  }
#if !defined(PS3)
  int nMax = 0;
  if (pSH->m_eSHClass == eHWSC_Vertex)
    nMax = MAX_CONSTANTS_VS;
  else
  if (pSH->m_eSHClass == eHWSC_Pixel)
    nMax = MAX_CONSTANTS_PS;
  else
    nMax = MAX_CONSTANTS_GS;
#else
	int nMax	=	MAX_CONSTANTS;
#endif
  assert(pInst->m_nMaxVecs[0] < nMax);
  assert(pInst->m_nMaxVecs[1] < nMax);

#endif

  if ((pInst->m_RTMask & (g_HWSR_MaskBit[HWSR_INSTANCING_ATTR] | g_HWSR_MaskBit[HWSR_INSTANCING_ROT])) && pSH->m_eSHClass == eHWSC_Vertex)
  {
    int nNumInst = 0;
    if (InstBindVars)
    {
      for (i=0; i<InstBindVars->size(); i++)
      {
        SCGBind *b = &(*InstBindVars)[i];
        int nID = b->m_dwBind;
        if (!nNumInst)
          pInst->m_nInstMatrixID = nID;
        bool bRes = true;

        SCGBind bn;
        bn.m_nParameters = b->m_nParameters;
        bn.m_dwBind = nID;
        bRes = mfAddFXParameter(pInst, pSH->m_Params, pSH->m_Samplers, b->m_Name.c_str(), b->m_Name.c_str(), &bn, true, pSH->m_eSHClass);

        nNumInst++;
      }
    }
    //assert(cgi->m_nNumInstAttributes == nNumInst);
    pInst->m_nNumInstAttributes = nNumInst;

    if (pInst->m_pParams_Inst)
      qsort(&(*pInst->m_pParams_Inst)[0], pInst->m_pParams_Inst->size(), sizeof(SCGParam), CGParamCallback);
  }
  if (pInst->m_pParams[0] && pInst->m_pParams[0]->size() > 1)
    qsort(&(*pInst->m_pParams[0])[0], pInst->m_pParams[0]->size(), sizeof(SCGParam), CGParamCallback);
  if (pInst->m_pParams[1] && pInst->m_pParams[1]->size() > 1)
    qsort(&(*pInst->m_pParams[1])[0], pInst->m_pParams[1]->size(), sizeof(SCGParam), CGParamCallback);
}

// Vertex shader specific
void CHWShader_D3D::mfPostVertexFormat(SHWSInstance *pInst, bool bCol, byte bNormal, bool bTC0, bool bTC1[2], bool bPSize, bool bTangent[2], bool bHWSkin, bool bSH[2], bool bShapeDeform, bool bMorphTarget, bool bMorph, bool bVertexScatter)
{
//#if defined(PS3) && !defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
//  assert(!bHWSkin && !bShapeDeform && !bMorphTarget && !bMorph);
//#endif

  if (bTC1[0])
    pInst->m_VStreamMask_Decl |= VSM_LMTC;
  if (bTC1[1])
    pInst->m_VStreamMask_Stream |= VSM_LMTC;

  if (bTangent[0])
    pInst->m_VStreamMask_Decl |= VSM_TANGENTS;
  if (bTangent[1])
    pInst->m_VStreamMask_Stream |= VSM_TANGENTS;

  if (bHWSkin)
  {
    pInst->m_VStreamMask_Decl |= VSM_HWSKIN;
    pInst->m_VStreamMask_Stream |= VSM_HWSKIN;
  }
  if (bSH[0])
    pInst->m_VStreamMask_Decl |= VSM_SH;
  if (bSH[1])
    pInst->m_VStreamMask_Stream |= VSM_SH;

  if (bShapeDeform)
  {
    pInst->m_VStreamMask_Decl |= VSM_HWSKIN_SHAPEDEFORM;
    pInst->m_VStreamMask_Stream |= VSM_HWSKIN_SHAPEDEFORM;
  }
  if (bMorphTarget)
  {
    pInst->m_VStreamMask_Decl |= VSM_HWSKIN_MORPHTARGET;
    pInst->m_VStreamMask_Stream |= VSM_HWSKIN_MORPHTARGET;
  }
  if (bMorph)
  {
    pInst->m_VStreamMask_Decl |= VSM_MORPHBUDDY;
    pInst->m_VStreamMask_Stream |= VSM_MORPHBUDDY;
  }
  if (bVertexScatter)
  {
    pInst->m_VStreamMask_Decl |= VSM_SCATTER;
    pInst->m_VStreamMask_Stream |= VSM_SCATTER;
  }

  int nVFormat;
  if (bNormal)
  {
    assert(!bTC0 && bCol);
    nVFormat = VERTEX_FORMAT_P3F_N4B_COL4UB;
  }
  else
  {
    nVFormat = VertFormatForComponents(bCol, bTC0, bPSize);
    if (nVFormat == VERTEX_FORMAT_P3F_TEX2F && CRenderer::CV_r_shadersalwaysusecolors)
      nVFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
  }
  pInst->m_nVertexFormat = nVFormat;
}

int CHWShader_D3D::mfVertexFormat(bool &bUseTangents, bool &bUseLM, bool &bUseHWSkin, bool& bUseSH)
{
  int i;

  assert (m_eSHClass == eHWSC_Vertex);

  int nVFormat = VERTEX_FORMAT_P3F;
  int nStream = 0;
  for (i=0; i<m_Insts.Num(); i++)
  {
    SHWSInstance *pInst = &m_Insts[i];
//#if defined(PS3) && !defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
//    assert((pInst->m_StreamMask & VSM_HWSKIN) == 0);
//#endif
    nVFormat = gRenDev->m_RP.m_VFormatsMerge[nVFormat][pInst->m_nVertexFormat];
    nStream |= pInst->m_VStreamMask_Stream;
  }
  bUseTangents = (nStream & VSM_TANGENTS) != 0;
  bUseLM = (nStream & VSM_LMTC) != 0;
  bUseHWSkin = (nStream & VSM_HWSKIN) != 0;
  bUseSH = (nStream & VSM_SH) != 0;
  assert (nVFormat < VERTEX_FORMAT_NUMS);

  return nVFormat;
}

void CHWShader_D3D::AddMissedInstancedParam(SHWSInstance *pInst, DynArray<SFXParam>& Params, int nIndex, DynArray<SCGBind>& InstBindVars)
{
  int i;
  for (i=0; i<InstBindVars.size(); i++)
  {
    SCGBind &b = InstBindVars[i];
    if (b.m_dwBind == nIndex)
    {
      SFXParam *pr = gRenDev->m_cEF.mfGetFXParameter(Params, b.m_Name.c_str());
      if (!pr)
      {
        if (!pInst->m_pParams_Inst)
          pInst->m_pParams_Inst = new std::vector<SCGParam>;
        SCGParam pr;
        pr.m_dwBind = b.m_dwBind;
        pr.m_Flags = b.m_Flags | PF_SINGLE_COMP;
        pr.m_Name = b.m_Name;
        pr.m_nParameters = b.m_nParameters;
        pr.m_eCGParamType = ECGP_Unknown;
        pInst->m_pParams_Inst->push_back(pr);
        break;
      }
    }
  }
}

#if !defined(PS3)

void CHWShader_D3D::AnalyzeSemantic(SHWSInstance *pInst, DynArray<SFXParam>& Params, D3DXSEMANTIC *pSM, bool bUsed, bool& bPos, byte& bNormal, bool bTangent[], bool& bHWSkin, bool& bShapeDeform, bool& bMorphTarget, bool& bBoneSpace, bool& bPSize, bool bSH[], bool& bMorph, bool& bTC0, bool bTC1[], bool& bCol, bool& bSecCol, bool& bVertexScatter, DynArray<SCGBind>& InstBindVars)
{
  switch (pSM->Usage)
  {
  case D3DDECLUSAGE_POSITION:
    if (pSM->UsageIndex == 0)
      bPos = true;
    //#ifndef PS3
    else
    if (pSM->UsageIndex == 3)
      bMorphTarget = true;
    else
    if (pSM->UsageIndex == 1 || pSM->UsageIndex == 2)
      bShapeDeform = true;
    else
    if (pSM->UsageIndex == 4)
      bHWSkin = true;
    else
    if (pSM->UsageIndex == 8)
      bMorph = true;
//#endif
    else
      assert(false);
    break;

  case D3DDECLUSAGE_NORMAL:
    bNormal = true;
    break;

  case D3DDECLUSAGE_TEXCOORD:
    if (pSM->UsageIndex == 0)
      bTC0 = true;
    else
    if (pSM->UsageIndex > 0 && (pInst->m_RTMask & g_HWSR_MaskBit[HWSR_INSTANCING_ATTR]))
    {
      AddMissedInstancedParam(pInst, Params, pSM->UsageIndex, InstBindVars);
    }
    else
    if (pSM->UsageIndex == 1)
    {
      bTC1[0] = true;
      bTC1[1] = bUsed;
    }
    else
    if (pSM->UsageIndex == 2)
			bVertexScatter = bUsed;
		else
    if (pSM->UsageIndex == 8)
      bMorph = true;
    break;

  case D3DDECLUSAGE_COLOR:
    if (pSM->UsageIndex == 0)
      bCol = true;
    else
    if (pSM->UsageIndex == 1)
    {
      //assert(0);
      bSecCol = true;
    }
    else
    if (pSM->UsageIndex == 2 || pSM->UsageIndex == 3)
    {
      bSH[0] = true;
      bSH[1] = bUsed;
    }
    else
    if (pSM->UsageIndex == 4)
      bShapeDeform = true;
    else
      assert(false);
    break;

    //#ifndef PS3
  case D3DDECLUSAGE_TANGENT:
  case D3DDECLUSAGE_BINORMAL:
    bTangent[0] = true;
    bTangent[1] = bUsed;
    break;

  case D3DDECLUSAGE_PSIZE:
    bPSize = true;
    break;

  case D3DDECLUSAGE_BLENDWEIGHT:
  case D3DDECLUSAGE_BLENDINDICES:
    if (pSM->UsageIndex == 0)
      bHWSkin = true;
    else
    if (pSM->UsageIndex == 1)
      bMorph = true;
    else
      assert(0);
    break;
  default:
    {
      assert(0);
    }
  }
}

bool sCreateSemantic(D3DXSEMANTIC& SM, bool& bUsed, char *sName, char *sIndex, char *sMask, char *sReg, char *sSys, char *sFormat, char *sUsed)
{
  bUsed = true;
  if (!sUsed[0])
    bUsed = false;

  memset(&SM, 0, sizeof(D3DXSEMANTIC));
  if (!strcmp(sName, "POSITION"))
    SM.Usage = D3DDECLUSAGE_POSITION;
  else
  if (!strcmp(sName, "TEXCOORD"))
    SM.Usage = D3DDECLUSAGE_TEXCOORD;
  else
  if (!strcmp(sName, "COLOR"))
    SM.Usage = D3DDECLUSAGE_COLOR;
  else
  if (!strcmp(sName, "TANGENT"))
    SM.Usage = D3DDECLUSAGE_TANGENT;
  else
  if (!strcmp(sName, "BINORMAL"))
    SM.Usage = D3DDECLUSAGE_BINORMAL;
  else
  if (!strcmp(sName, "PSIZE"))
    SM.Usage = D3DDECLUSAGE_PSIZE;
  else
  if (!strcmp(sName, "BLENDWEIGHT"))
    SM.Usage = D3DDECLUSAGE_BLENDWEIGHT;
  else
  if (!strcmp(sName, "BLENDINDICES"))
    SM.Usage = D3DDECLUSAGE_BLENDINDICES;
  else
  if (!strcmp(sName, "NORMAL"))
    SM.Usage = D3DDECLUSAGE_NORMAL;
  else
  {
    assert(0);
    return false;
  }
  SM.UsageIndex = shGetInt(sIndex);

  return true;
}
#endif


static bool sGetStr(char *& sS, char *szDst)
{
  int n = 0;
  szDst[n] = 0;
  while(sS[0]==0x20 || sS[0]==8) { sS++; }
  if (sS[0]=='\n')
    return false;
  if (!sS[0])
    return false;
  shFill(&sS, szDst, 32);
  SkipCharacters(&sS, kWhiteSpace);
  return true;
}

int CHWShader_D3D::mfVertexFormat(SHWSInstance *pInst, CHWShader_D3D *pSH, LPD3DXBUFFER pShader, DynArray<SCGBind>& InstBindVars)
{
  assert (pSH->m_eSHClass == eHWSC_Vertex);

  byte bNormal = false;
  bool bTangent[2] = {false, false};
  bool bHWSkin = false;
  bool bShapeDeform = false;
  bool bMorphTarget = false;
  bool bMorph = false;
  bool bBoneSpace = false;
  bool bPSize = false;
  bool bSH[2] = {false, false};
  bool bTC0 = false;
  bool bTC1[2] = {false, false};
  bool bCol = false;
  bool bSecCol = false;
  bool bPos = false;
	bool bVertexScatter = false;
  int nVFormat = VERTEX_FORMAT_P3F;

#if defined (DIRECT3D9) || defined (OPENGL)

  D3DXSEMANTIC Semantics[MAXD3DDECLLENGTH];
  uint nCounts;

  if (!CParserBin::m_bD3D10)
  {
    HRESULT hr = D3DXGetShaderInputSemantics((DWORD *)pShader->GetBufferPointer(), Semantics, &nCounts);
    assert(SUCCEEDED(hr));
    if (!FAILED(hr))
    {
      for (int i=0; i<nCounts; i++)
      {
        D3DXSEMANTIC *pSM = &Semantics[i];
        if (pSM->UsageIndex == (unsigned)-1)
          continue;
        AnalyzeSemantic(pInst, pSH->m_Params, pSM, true, bPos, bNormal, bTangent, bHWSkin, bShapeDeform, bMorphTarget, bBoneSpace, bPSize, bSH, bMorph, bTC0, bTC1, bCol, bSecCol, bVertexScatter, InstBindVars);
      }
    }
    mfPostVertexFormat(pInst, bCol, bNormal, bTC0, bTC1, bPSize, bTangent, bHWSkin, bSH, bShapeDeform, bMorphTarget, bMorph, bVertexScatter);
  }
  else
  {
    char *pB = (char *)pShader->GetBufferPointer();
    char *sS = strstr(pB, "Input signature:");
    assert(sS);
    sS = strstr(sS, "-----");
    assert(sS);
    while(*sS != '\n') {sS++;}
    sS++;
    while (true)
    {
      char sSrc[256], sC[8], sName[32], sIndex[8], sMask[8], sReg[8], sSys[8], sFormat[8], sUsed[8];
      char *pS = sSrc;
      fxFillCR(&sS, sSrc);
      sGetStr(pS, sC);
      if (!sGetStr(pS, sName))
        break;
      sGetStr(pS, sIndex);
      sGetStr(pS, sMask);
      sGetStr(pS, sReg);
      sGetStr(pS, sSys);
      sGetStr(pS, sFormat);
      sGetStr(pS, sUsed);
      assert(sC[0] == '/');
      D3DXSEMANTIC SM;
      bool bUsed;
      bool bRes = sCreateSemantic(SM, bUsed, sName, sIndex, sMask, sReg, sSys, sFormat, sUsed);
      if (bRes)
        AnalyzeSemantic(pInst, pSH->m_Params, &SM, bUsed, bPos, bNormal, bTangent, bHWSkin, bShapeDeform, bMorphTarget, bBoneSpace, bPSize, bSH, bMorph, bTC0, bTC1, bCol, bSecCol, bVertexScatter, InstBindVars);
    }
    mfPostVertexFormat(pInst, bCol, bNormal, bTC0, bTC1, bPSize, bTangent, bHWSkin, bSH, bShapeDeform, bMorphTarget, bMorph, bVertexScatter);
  }

#elif defined (DIRECT3D10)
  ID3D10ShaderReflection *pShaderReflection;
  UINT nSize = pShader->GetBufferSize();
  void *pData = pShader->GetBufferPointer();
  HRESULT hr = D3D10ReflectShader(pData, nSize, &pShaderReflection);
  assert (SUCCEEDED(hr));
  if (!SUCCEEDED(hr))
    return 0;
  D3D10_SHADER_DESC Desc;
  pShaderReflection->GetDesc(&Desc);
  if (!Desc.InputParameters)
    return 0;
  D3D10_SIGNATURE_PARAMETER_DESC IDesc;
  for (int i=0; i<Desc.InputParameters; i++)
  {
    pShaderReflection->GetInputParameterDesc(i, &IDesc);
    //if (!IDesc.ReadWriteMask)
    //  continue;
    if (!IDesc.SemanticName)
      continue;
    int nIndex;
    if (!strnicmp(IDesc.SemanticName, "POSITION", 8) || !strnicmp(IDesc.SemanticName, "SV_POSITION", 11))
    {
      nIndex = IDesc.SemanticIndex;
      if (nIndex == 0)
        bPos = true;
      else
      if (nIndex == 3)
        bMorphTarget = true;
      else
      if (nIndex == 1 || nIndex == 2)
        bShapeDeform = true;
      else
      if (nIndex == 4)
        bHWSkin = true;
      else
      if (nIndex == 8)
        bMorph = true;
      else
        assert(false);
    }
    else
    if (!strnicmp(IDesc.SemanticName, "NORMAL", 6))
    {
      bNormal = true;
    }
    else
    if (!strnicmp(IDesc.SemanticName, "TEXCOORD", 8))
    {
      nIndex = IDesc.SemanticIndex;
      if (nIndex == 0)
        bTC0 = true;
      else
      {
        if (nIndex > 0 && (pInst->m_RTMask & g_HWSR_MaskBit[HWSR_INSTANCING_ATTR]))
        {
          AddMissedInstancedParam(pInst, pSH->m_Params, nIndex, InstBindVars);
        }
        else
        if (nIndex == 1)
        {
          bTC1[0] = true;
          if (IDesc.ReadWriteMask)
            bTC1[1] = true;
        }
        else
        if (nIndex == 2)
        {
          if (IDesc.ReadWriteMask)
            bVertexScatter = true;
        }
        else
        if (nIndex == 8)
          bMorph = true;
      }
    }
    else
    if (!strnicmp(IDesc.SemanticName, "COLOR", 5))
    {
      nIndex = IDesc.SemanticIndex;
      if (nIndex == 0)
        bCol = true;
      else
      if (nIndex == 1)
        bSecCol = true;
      else
      {
        if (nIndex == 2 || nIndex == 3)
        {
          bSH[0] = true;
          if (IDesc.ReadWriteMask)
            bSH[1] = true;
        }
        else
        if (nIndex == 4)
          bShapeDeform = true;
        else
          assert(false);
      }
    }
    else
    if (!stricmp(IDesc.SemanticName, "TANGENT") || !stricmp(IDesc.SemanticName, "BINORMAL"))
    {
      bTangent[0] = true;
      if (IDesc.ReadWriteMask)
        bTangent[1] = true;
    }
    else
    if (!strnicmp(IDesc.SemanticName, "PSIZE", 5))
    {
      bPSize = true;
    }
    else
    if (!strnicmp(IDesc.SemanticName, "BLENDWEIGHT", 11) || !strnicmp(IDesc.SemanticName, "BLENDINDICES", 12))
    {
      nIndex = IDesc.SemanticIndex;
      if (nIndex == 0)
        bHWSkin = true;
      else
      if (nIndex == 1)
        bMorph = true;
      else
        assert(0);
    }
    else
    {
      assert(0);
    }
  }
  mfPostVertexFormat(pInst, bCol, bNormal, bTC0, bTC1, bPSize, bTangent, bHWSkin, bSH, bShapeDeform, bMorphTarget, bMorph, bVertexScatter);
  SAFE_RELEASE(pShaderReflection);
#endif
  return pInst->m_nVertexFormat;
}

const SWaveForm sWFX = SWaveForm(eWF_Sin, 0, 3.5f, 0, 0.2f);
const SWaveForm sWFY = SWaveForm(eWF_Sin, 0, 5.0f, 90.0f, 0.2f);

union UFloat4
{
  float f[4];
#if defined(_CPU_SSE)
  __m128 m128;
#endif
};

DEFINE_ALIGNED_DATA(UFloat4, sData[32], 16);
DEFINE_ALIGNED_DATA(float, sTempData[32][4], 16);
DEFINE_ALIGNED_DATA(float, sMatrInstData[3][4], 16);

void sTranspose(Matrix34& m, Matrix44 *dst)
{
  dst->m00=m.m00;	dst->m01=m.m10;	dst->m02=m.m20;	dst->m03=0;
  dst->m10=m.m01;	dst->m11=m.m11;	dst->m12=m.m21;	dst->m13=0;
  dst->m20=m.m02;	dst->m21=m.m12;	dst->m22=m.m22;	dst->m23=0;
  dst->m30=m.m03;	dst->m31=m.m13;	dst->m32=m.m23;	dst->m33=1;
}
void sTranspose(Matrix34& m, Matrix33 *dst)
{
  dst->m00=m.m00;	dst->m01=m.m10;	dst->m02=m.m20;
  dst->m10=m.m01;	dst->m11=m.m11;	dst->m12=m.m21;
  dst->m20=m.m02;	dst->m21=m.m12;	dst->m22=m.m22;
}

namespace
{

  float* sGetLightMatrix(CD3D9Renderer *r)
  {
    SLightPass *pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
    assert (pLP->nLights==1 && (pLP->pLights[0]->m_Flags & DLF_PROJECT));
    CDLight *pDL = pLP->pLights[0];
    if (pDL && pDL->m_pLightImage)
    {
      //*(Matrix44 *)&sData[0].f[0] = pDL->m_ProjMatrix;
      mathMatrixTranspose(&sData[0].f[0], pDL->m_ProjMatrix.GetData(), g_CpuFlags);
    }
    else
      memcpy(&sData[0].f[0], &sIdentityMatrix, 4*4*4);
    return &sData[0].f[0];
  }
  CRendElement *sGetContainerRE0(CRendElement * pRE)
  {
    assert(pRE);		// someone assigned wrong shader - function should not be called then

    if(pRE->mfGetType() == eDATA_Mesh && ((CREMesh*)pRE)->m_pRenderMesh->m_pVertexContainer)
    {
      assert(((CREMesh*)pRE)->m_pRenderMesh->m_pVertexContainer->m_pChunks->Count()>=1);
      return ((CREMesh*)pRE)->m_pRenderMesh->m_pVertexContainer->m_pChunks->Get(0)->pRE;
    }

    return pRE;
  }

  float *sGetTerrainBase(CD3D9Renderer *r)
  {
    if(!r->m_RP.m_pRE)  
      return NULL;				// it seems the wrong material was assigned 

    // use render element from vertex container render mesh if available
    CRendElement *pRE = sGetContainerRE0(r->m_RP.m_pRE);

    if (pRE->m_CustomData)
    {
      float *pData;

      if(SRendItem::m_RecurseLevel<=1)
        pData = (float *)pRE->m_CustomData;
      else
        pData = (float *)pRE->m_CustomData + 4;

      sData[0].f[0] = pData[2]; sData[0].f[1] = pData[0]; sData[0].f[2] = pData[1]; sData[0].f[3] = 0;
      return &sData[0].f[0];
    }
    else
      return NULL;
  }
  float *sGetTerrainLayerGen(CD3D9Renderer *r)
  {
    if(!r->m_RP.m_pRE)
      return NULL;				// it seems the wrong material was assigned 

    CRendElement *pRE = r->m_RP.m_pRE;

    float *pData = (float *)pRE->m_CustomData;
    if (pData)
    {
      sData[0].f[0] = pData[0]; sData[0].f[1] = pData[1]; sData[0].f[2] = pData[2]; sData[0].f[3] = pData[3];
      sData[1].f[0] = pData[4]; sData[1].f[1] = pData[5]; sData[1].f[2] = pData[6]; sData[1].f[3] = 0;
      sData[2].f[0] = pData[8]; sData[2].f[1] = pData[9]; sData[2].f[2] = pData[10]; sData[2].f[3] = pData[11];
      sData[3].f[0] = pData[12]; sData[3].f[1] = pData[13]; sData[3].f[2] = pData[14]; sData[3].f[3] = pData[15];
      return &sData[0].f[0];
    }
    else
      return NULL;
  }
  float *sGetTexMatrix(CD3D9Renderer *r, const SCGParam *ParamBind)
  {
    static int nLastObjFrame=-1;
    static Vec3 pLastPos;
    static Ang3 pLastAngs;
    static CTexture *pLastTex;
    DEFINE_ALIGNED_DATA_STATIC(Matrix44, m, 16);

    CTexture *tp = NULL;
    SHRenderTarget *pTarg = (SHRenderTarget *)(UINT_PTR)ParamBind->m_nID;
    assert(pTarg);
    if (!pTarg)
      return NULL;
    SEnvTexture *pEnvTex = pTarg->GetEnv2D();
    //assert(pEnvTex && pEnvTex->m_pTex);
    if (!pEnvTex || !pEnvTex->m_pTex)
      return NULL;
    if (pEnvTex->m_pTex)
      tp = pEnvTex->m_pTex->m_pTexture;
    if (!tp)
      return NULL;
    //assert(tp->m_Matrix);
    if (tp->m_Matrix)
    {
      if (r->m_RP.m_FrameObject != nLastObjFrame || pLastTex != tp || (!r->GetCamera().GetAngles().IsEquivalent(pLastAngs,VEC_EPSILON)) || (!IsEquivalent(r->GetCamera().GetPosition(),pLastPos,VEC_EPSILON)))
      {
        pLastTex = tp;
        pLastPos = r->GetCamera().GetPosition();
        pLastAngs = r->GetCamera().GetAngles();
        nLastObjFrame = r->m_RP.m_FrameObject;
        
        if ((pTarg->m_eUpdateType != eRTUpdate_WaterReflect) && r->m_RP.m_pCurObject->m_ObjFlags & FOB_TRANS_MASK)
          m = r->m_RP.m_pCurObject->m_II.m_Matrix * *(Matrix44 *)tp->m_Matrix;
        else
          memcpy(m.GetData(), tp->m_Matrix, 4*4*sizeof(float));

        m.Transpose();
      }

    }
    return m.GetData();
  }
  void sGetWind(CD3D9Renderer *r)
  {
    static int nLastObjFrame=-1;    
    static Vec4 pWind( 0, 0, 0, 0 );    
        
    if ( r->m_RP.m_FrameObject != nLastObjFrame )
    {
      nLastObjFrame = r->m_RP.m_FrameObject;

      CRenderObject *pObj = r->m_RP.m_pCurObject;
      pWind.x = pObj->m_vBending.x;
      pWind.y = pObj->m_vBending.y;
      
      // Get phase variation based on object id
      pWind.z = (float) ((int) pObj->m_pID)/ (float) (INT_MAX);
      pWind.z *= 100000.0f;
      pWind.z -= floorf( pWind.z );
      pWind.z *= 10.0f;

      pWind.w = pObj->m_vBending.GetLength();
    }

    sData[0].f[0] = pWind.x;
    sData[0].f[1] = pWind.y;
    sData[0].f[2] = pWind.z;
    sData[0].f[3] = pWind.w;    
  }
  void sGetRotGridScreenOff(CD3D9Renderer *r)
  {
    int iTempX, iTempY, iWidth, iHeight;
    r->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
    sData[0].f[0] = 1.0f / (float)iWidth;
    sData[0].f[1] = 0.f; 
    sData[0].f[2] = 0.f;
    sData[0].f[3] = 1.0f / (float)iHeight;
    //rotated grid    
    Vec4 t75 = Vec4(0.75f * sData[0].f[0], 0.75f * sData[0].f[1], 0.75f * sData[0].f[2], 0.75f * sData[0].f[3]);
    Vec4 t25 = Vec4(0.25f * sData[0].f[0], 0.25f * sData[0].f[1], 0.25f * sData[0].f[2], 0.25f * sData[0].f[3]);
    Vec2 rotX = Vec2(t75[0]+t25[2], t75[1]+t25[3]);
    Vec2 rotY = Vec2(t75[2]-t25[0], t75[3]-t25[1]); 
    sData[0].f[0] = rotX[0];     sData[0].f[1] = rotX[1];
    sData[0].f[2] = rotY[0];     sData[0].f[3] = rotY[1];
  }

  float sGetMaterialLayersOpacity( CD3D9Renderer *r )
  {
    float fMaterialLayersOpacity = 1.0f;

    uint32 nResourcesNoDrawFlags = r->m_RP.m_pShaderResources->GetMtlLayerNoDrawFlags();
    if( (r->m_RP.m_pCurObject->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK) && !(nResourcesNoDrawFlags&MTL_LAYER_CLOAK) )
    {
      fMaterialLayersOpacity = ((float)((r->m_RP.m_pCurObject->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK)>> 8) / 255.0f);
      fMaterialLayersOpacity = min(1.0f, 4.0f * max( 1.0f - fMaterialLayersOpacity, 0.0f) ); 
    }

    return fMaterialLayersOpacity;
  }

  void sGetScreenSize(CD3D9Renderer *r)
  {
    int iTempX, iTempY, iWidth, iHeight;
    r->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
    sData[0].f[0] = (float)iWidth;
    sData[0].f[1] = (float)iHeight;
    sData[0].f[2] = 0.5f/(float)iWidth;
    sData[0].f[3] = 0.5f/(float)iHeight;
  }
  void sGetIrregKernel(CD3D9Renderer *r)
  {
#define PACKED_SAMPLES 1
    //samples for cubemaps
    /*const Vec4 irreg_kernel[8]=
    {
      Vec4(0.527837f, -0.085868f, 0.527837f, 0),
      Vec4(-0.040088f, 0.536087f, -0.040088f, 0),
      Vec4(-0.670445f, -0.179949f, -0.670445f, 0),
      Vec4(-0.419418f, -0.616039f, -0.419418f, 0),
      Vec4(0.440453f, -0.639399f, 0.440453f, 0),
      Vec4(-0.757088f, 0.349334f, -0.757088f, 0),
      Vec4(0.574619f, 0.685879f, 0.574619f, 0),
      Vec4(0.03851f, -0.939059f, 0.03851f, 0)
    };
    f32 fFrustumScale = r->m_cEF.m_TempVecs[4][0]; //take only first cubemap 
    for (int i=0; i<8; i++)
    {
      sData[i].f[0] = irreg_kernel[i][0] * (1.0f/fFrustumScale);
      sData[i].f[1] = irreg_kernel[i][1] * (1.0f/fFrustumScale);
      sData[i].f[2] = irreg_kernel[i][2] * (1.0f/fFrustumScale);
      sData[i].f[3] = 0;
    }*/

    int nSamplesNum = 1;
    switch (r->m_RP.m_nShaderQuality)
    {
      case eSQ_Medium:
      case eSQ_High:
        nSamplesNum = 8;
        break;
      case eSQ_VeryHigh:
        nSamplesNum = 16;
        break;
    }

    CPoissonDiskGen::SetKernelSize(nSamplesNum);

#ifdef PACKED_SAMPLES
    for (int i=0, nIdx=0; i<nSamplesNum; i+=2, nIdx++)
    {
      Vec2 vSample = CPoissonDiskGen::GetSample(i);
      sData[nIdx].f[0] = vSample.x;
      sData[nIdx].f[1] = vSample.y;
      vSample = CPoissonDiskGen::GetSample(i+1);
      sData[nIdx].f[2] = vSample.x;
      sData[nIdx].f[3] = vSample.y;
    }
#else
    for (int i=0, nIdx=0; i<nSamplesNum; i++, nIdx++)
    {
      Vec2 vSample = CPoissonDiskGen::GetSample(i); 
      sData[nIdx].f[0] = vSample.x;
      sData[nIdx].f[1] = vSample.y;
      sData[nIdx].f[2] = 0.0f;
      sData[nIdx].f[3] = 0.0f;
    }
#endif

#undef PACKED_SAMPLES

  }

  void sGetRegularKernel(CD3D9Renderer *r)
  {

    float fRadius = r->CV_r_shadow_jittering;
    float SHADOW_SIZE = 1024.f;

    const Vec4 regular_kernel[9]=
    {
      Vec4(-1, 1, 0, 0),
      Vec4( 0, 1, 0, 0),
      Vec4( 1, 1, 0, 0),
      Vec4(-1, 0, 0, 0),
      Vec4( 0, 0, 0, 0),
      Vec4( 1, 0, 0, 0),
      Vec4(-1, -1, 0, 0),
      Vec4( 0, -1, 0, 0),
      Vec4( 1, -1, 0, 0)
    };

    float fFilterRange = fRadius/SHADOW_SIZE;

    for (int32 nInd = 0; nInd<9; nInd++)
    {
        if ((nInd%2) == 0)
        {
          sData[nInd/2].f[0] = regular_kernel[nInd].x * fFilterRange;
          sData[nInd/2].f[1] = regular_kernel[nInd].y * fFilterRange;
        }
        else
        {
          sData[nInd/2].f[2] = regular_kernel[nInd].x * fFilterRange;;
          sData[nInd/2].f[3] = regular_kernel[nInd].y * fFilterRange;;
        }
    }

    /*float FilterRange = radius/SHADOW_SIZE;
    float FilterStep = 1.0f/SHADOW_SIZE;

    int32 nInd = 0;
    for (float y=-FilterRange; y<FilterRange; y+=FilterStep)
    {
      for (float x=-FilterRange; x<FilterRange; x+=FilterStep, ++nInd)
      {
        assert((nInd/2) <= 5);

        if ((nInd%2) == 0)
        {
          sData[nInd/2][0] = x;
          sData[nInd/2][1] = y;
        }
        else
        {
          sData[nInd/2][2] = x;
          sData[nInd/2][3] = y;
        }
      }
    }
    */
  }

  void sGetBendInfo(CD3D9Renderer *r)
  {
    static int nLastObjFrame=-1;    
    static Vec4 pLastBending( 0, 0, 0, 0 );    

    if ( r->m_RP.m_FrameObject != nLastObjFrame )
    {
      nLastObjFrame = r->m_RP.m_FrameObject;

      CRenderObject *pObj = r->m_RP.m_pCurObject;
      pLastBending = Vec4(0, 0, 0, 0);

      if (pObj && fabs_tpl(pObj->m_vBending.x + pObj->m_vBending.y + pObj->m_fBendScale) > 0.0f )
      {
        if (pObj->m_fBendScale == 0.0f)
        {          
          pLastBending.z = pObj->m_vBending.GetLength();
          pLastBending.x = pLastBending.y = pLastBending.w = 0.0f;
        }
        else
        {
          // Wind affected bending
          pLastBending.x = pObj->m_vBending.x;
          pLastBending.y = pObj->m_vBending.y;
          pLastBending.z = pObj->m_fBendScale;
        }

        if (pObj->m_nWaveID > 0)
        {
          pLastBending.x += CShaderMan::EvalWaveForm(&CRenderObject::m_Waves[pObj->m_nWaveID]);
          pLastBending.y += CShaderMan::EvalWaveForm(&CRenderObject::m_Waves[pObj->m_nWaveID+1]) ;          
        }

        
        if (pObj->m_fBendScale == 0.f)
        {
          // This values are getting scaled down in shader (due to wind version), rescale them back
          pLastBending.x *= 50.0f;
          pLastBending.y *= 50.0f;

          pLastBending.w = pObj->m_vBending.GetLength();
          pLastBending.z = pLastBending.w;

          // Must scale down wind strength (due to wind version)
          pLastBending.w *= 0.25f;
        }
        else
        {
          pLastBending.w = Vec2(pLastBending.x, pLastBending.y).GetLength();
        }
      }
      else
      {
        pLastBending = Vec4(0,0,0,0);
      }
    } 
    sData[0].f[0] = pLastBending.x;
    sData[0].f[1] = pLastBending.y;
    sData[0].f[2] = pLastBending.z;
    sData[0].f[3] = pLastBending.w;
  }

  Vec4 sGetVolumetricFogParams(CD3D9Renderer *r)
  {
    static int nFrameID = 0;
    static Vec4 pFogParams = Vec4(0,0,0,0);

    //if( nFrameID != gRenDev->GetFrameID() )
    {
      I3DEngine *pEng = iSystem->GetI3DEngine();  

      float globalDensity(0); float atmosphereHeight(0); float artistTweakDensityOffset(0); float globalDensityMultiplierLDR(1);
      pEng->GetVolumetricFogSettings(globalDensity, atmosphereHeight, artistTweakDensityOffset, globalDensityMultiplierLDR);

      float globalDensityMod(0); float atmosphereHeightMod(0);
      pEng->GetVolumetricFogModifiers(globalDensityMod, atmosphereHeightMod);

      globalDensity += globalDensityMod;		
      globalDensity	= ( globalDensity < 0.0f ) ? 0.0f : globalDensity;
      atmosphereHeight += atmosphereHeightMod;
      atmosphereHeightMod = ( atmosphereHeight < 1.0f ) ? 1.0f : atmosphereHeight;

      float globalDensityMultiplier(1);
      pEng->GetVolumetricFogMultipliers(globalDensityMultiplier);

      globalDensity *= globalDensityMultiplier;

			if (!gRenDev->EF_Query(EFQ_HDRModeEnabled))
				globalDensity *= globalDensityMultiplierLDR;

      float atmosphereScale( 16.0f / atmosphereHeight ); // used as an argument for exp( -a * x ); if x >= AtmosphereHeight then the density for the given height will be close to zero
      float viewerHeight(r->GetRCamera().Orig.z);
      float waterLevel(pEng->GetWaterLevel());
      if( fabsf( waterLevel - WATER_LEVEL_UNKNOWN ) < 1e-4 )
        waterLevel = 0.0f;

      globalDensity *= 0.01f; // multiply by 1/100 to scale value editor value back to a reasonable range

      pFogParams.x = atmosphereScale;
      pFogParams.y = 1.44269502f * globalDensity * exp( -atmosphereScale * ( viewerHeight - waterLevel ) ); // log2(e) = 1.44269502
      pFogParams.z = globalDensity; 
      pFogParams.w = artistTweakDensityOffset; 

      nFrameID = gRenDev->GetFrameID();
    }
    sData[0].f[0] = pFogParams.x; 
    sData[0].f[1] = pFogParams.y; 
    sData[0].f[2] = pFogParams.z; 
    sData[0].f[3] = pFogParams.w;

    return pFogParams;
  }

  float *sTranspose(Matrix34& m)
  {
    static Matrix44 dst;
    dst.m00=m.m00;	dst.m01=m.m10;	dst.m02=m.m20;	dst.m03=0;
    dst.m10=m.m01;	dst.m11=m.m11;	dst.m12=m.m21;	dst.m13=0;
    dst.m20=m.m02;	dst.m21=m.m12;	dst.m22=m.m22;	dst.m23=0;
    dst.m30=m.m03;	dst.m31=m.m13;	dst.m32=m.m23;	dst.m33=1;

    return dst.GetData();
  }
  void sTranspose(Matrix44& m, Matrix44 *dst)
  {
    dst->m00=m.m00;	dst->m01=m.m10;	dst->m02=m.m20;	dst->m03=m.m30;
    dst->m10=m.m01;	dst->m11=m.m11;	dst->m12=m.m21;	dst->m13=m.m31;
    dst->m20=m.m02;	dst->m21=m.m12;	dst->m22=m.m22;	dst->m23=m.m32;
    dst->m30=m.m03;	dst->m31=m.m13;	dst->m32=m.m23;	dst->m33=m.m33;
  }

  void sGetMotionBlurData(CD3D9Renderer *r)
  {
    // special motion blur instanced data:
    //  - contains (previous frame camera view) * (previous frame object matrix) 
    //  - merged transformation with object motion blur amount and shutter speed

    static int nLastObjFrame=-1;    
    static Matrix44 mCamObjCurr;
    
    static int nLastFrameID=-1;    
    static Vec3 pCamPrevPos = Vec3(0,0,0);

    if( nLastFrameID != gRenDev->GetFrameID(true) )
    {
      // invert and get camera previous world space position
      Matrix44 CamPrevMatInv = r->m_CameraMatrixPrev.GetInverted();
      pCamPrevPos = CamPrevMatInv.GetRow(3);

      nLastFrameID = gRenDev->GetFrameID(true);
    }

    if ( r->m_RP.m_FrameObject != nLastObjFrame )
    {
      float fMotionBlurAmount( 0.8f );
      nLastObjFrame = r->m_RP.m_FrameObject;

      CRenderObject *pObj = r->m_RP.m_pCurObject;

      Matrix44 mObjCurr;
      GetTransposed44(pObj->m_II.m_Matrix, mObjCurr);   
      Matrix44 mObjPrev;
      GetTransposed44(pObj->m_prevMatrix, mObjPrev);   

      Matrix44 mCamObjPrev;
      
      if ( (r->m_RP.m_ObjFlags & FOB_CAMERA_SPACE))
      {
        Vec3 pCamCurrPos = r->GetRCamera().Orig;

        // set correct translation
        Vec3 pTranslation = pObj->m_II.m_Matrix.GetTranslation();
        pTranslation += pCamCurrPos;
        mObjCurr.SetRow(3, pTranslation );

        pTranslation = pObj->m_prevMatrix.GetTranslation();
        pTranslation += pCamPrevPos;
        mObjPrev.SetRow(3, pTranslation );
      }

      mathMatrixMultiply(mCamObjCurr.GetData(), r->m_CameraMatrix.GetData(), mObjCurr.GetData(), g_CpuFlags); 
      mathMatrixMultiply(mCamObjPrev.GetData(), r->m_CameraMatrixPrev.GetData(), mObjPrev.GetData(), g_CpuFlags); 
      float fExposureTime = CRenderer::CV_r_motionblur_shutterspeed * fMotionBlurAmount;

      // mk fix for time normalization

      // renormalize frametime to account for time scaling
      float fCurrFrameTime = gEnv->pTimer->GetFrameTime();      
      float fTimeScale = gEnv->pTimer->GetTimeScale();
      if (fTimeScale < 1.0f)
      {
        fTimeScale = max(0.0001f, fTimeScale);
        fCurrFrameTime /= fTimeScale; 
      }

      float fAlpha = 0.0f;
			if(fCurrFrameTime != 0.0f)
				fAlpha = fExposureTime / fCurrFrameTime;  

      if( CRenderer::CV_r_motionblurframetimescale )
      {
        float fAlphaScale = iszero(fCurrFrameTime) ? 1.0f : min(1.0f, (1.0f / fCurrFrameTime) / ( 32.0f)); // attenuate motion blur for lower frame rates
        fAlpha *= fAlphaScale;
      }

      mCamObjCurr = mCamObjCurr * (1.0f - fAlpha) + mCamObjPrev * fAlpha; 
      mCamObjCurr.Transpose();
    }

    float *pData = mCamObjCurr.GetData();
    // todo: use SSE
    sData[0].f[0] = pData[0]; sData[0].f[1] = pData[1]; sData[0].f[2] = pData[2]; sData[0].f[3] = pData[3];
    sData[1].f[0] = pData[4]; sData[1].f[1] = pData[5]; sData[1].f[2] = pData[6]; sData[1].f[3] = pData[7];
    sData[2].f[0] = pData[8]; sData[2].f[1] = pData[9]; sData[2].f[2] = pData[10]; sData[2].f[3] = pData[11];
  }

  void sGetCloakMotionData(CD3D9Renderer *r)
  {
		// todo: remove this

		sData[0].f[0] = 1.0f; sData[0].f[1] = 0.0f; sData[0].f[2] = 0.0f; sData[0].f[3] = 0.0f;
		sData[1].f[0] = 0.0f; sData[1].f[1] = 1.0f; sData[1].f[2] = 0.0f; sData[1].f[3] = 0.0f;
		sData[2].f[0] = 0.0f; sData[2].f[1] = 0.0f; sData[2].f[2] = 1.0f; sData[2].f[3] = 0.0f;  
  }

  void sGetCloakParams(CD3D9Renderer *r)
  {
    CRenderObject *pObj = r->m_RP.m_pCurObject;
    static int nLastObjFrame=-1;  
		static int nLastFrameID=-1;
    static Vec4 pCloakParams;
		static Matrix44 mCamObjCurr;    
		static Vec3 pCamPrevPos = Vec3(0,0,0);

		if( nLastFrameID != gRenDev->GetFrameID(true) )
		{
			// invert and get camera previous world space position
			Matrix44 CamPrevMatInv = r->m_CameraMatrixPrev.GetInverted();
			pCamPrevPos = CamPrevMatInv.GetRow(3);
			nLastFrameID = gRenDev->GetFrameID(true);
		}

    if ( r->m_RP.m_FrameObject != nLastObjFrame  && pObj)
    {
      nLastObjFrame = r->m_RP.m_FrameObject;

      Vec3 pObjPosWS = pObj->m_II.m_Matrix.GetColumn(3);

      // Get amount of light on cpu - dont want to add more shader permutations - this might be useful for other stuff - expose
      float fLightAmount = 0.0f;
      for( int nCurrDLight = 0; nCurrDLight < r->m_RP.m_DLights[SRendItem::m_RecurseLevel].Num() ; ++nCurrDLight )
      {
        if ( pObj->m_DynLMMask & (1<<nCurrDLight) )
        {
          CDLight *pDL = r->m_RP.m_DLights[SRendItem::m_RecurseLevel][nCurrDLight];

          // compute attenuation if not sun
          float fAttenuation = 1.0f;
          if( !(pDL->m_Flags & DLF_SUN) )
          {
            float fInvRadius = pDL->m_fRadius;
            if (fInvRadius <= 0)
              fInvRadius = 1.f;

            fInvRadius = 1.f / fInvRadius;

            // light position
            Vec3 pLightVec = pDL->m_Origin - pObjPosWS;

            // compute attenuation
            pLightVec *= fInvRadius;
            fAttenuation = clamp_tpl<float>(1.0f - (pLightVec.x * pLightVec.x + pLightVec.y * pLightVec.y + pLightVec.z * pLightVec.z), 0.0f, 1.0f);
          }

          fLightAmount += fAttenuation * ( (pDL->m_Color[0] + pDL->m_Color[1] + pDL->m_Color[2]) *0.33f );
        }
      }

      // Add ambient
      ColorF &pAmbient = r->m_RP.m_pCurInstanceInfo->m_AmbColor;
      fLightAmount += (pAmbient[0] + pAmbient[1] + pAmbient[2])*0.33f;

      // Get cloak blend amount from material layers
      float fCloakBlendAmount = ((pObj->m_nMaterialLayers&0x0000ff00)>>8)/255.0f;

			// Get instance speed 
			float fMotionBlurAmount( 0.3f ); 
			nLastObjFrame = r->m_RP.m_FrameObject;

			CRenderObject *pObj = r->m_RP.m_pCurObject;

			Matrix44 mObjCurr = pObj->m_II.m_Matrix;
			mObjCurr.Transpose();   
			Matrix44 mObjPrev = pObj->m_prevMatrix;
			mObjPrev.Transpose();   

			Matrix44 mCamObjPrev;
			float fSpeedScale = 1.0f;
			if ( (r->m_RP.m_ObjFlags & FOB_CAMERA_SPACE))
			{
				Vec3 pCamCurrPos = r->GetRCamera().Orig;

				// set correct translation
				Vec3 pTranslation = pObj->m_II.m_Matrix.GetTranslation();
				pTranslation += pCamCurrPos;
				mObjCurr.SetRow(3, pTranslation );

				pTranslation = pObj->m_prevMatrix.GetTranslation();
				pTranslation += pCamPrevPos;
				mObjPrev.SetRow(3, pTranslation );

				// make it less visible in first person
				fSpeedScale *= 0.25f;
			}

			mCamObjCurr = mObjCurr;
			mCamObjPrev = mObjPrev;

			// temporary solution for GC demo
			float fExposureTime = 0.0005f; //CRenderer::CV_r_motionblur_shutterspeed * fMotionBlurAmount;

			// renormalize frametime to account for time scaling
			float fCurrFrameTime = gEnv->pTimer->GetFrameTime();      
			float fTimeScale = gEnv->pTimer->GetTimeScale();
			if (fTimeScale < 1.0f)
			{
				fTimeScale = max(0.0001f, fTimeScale);
				fCurrFrameTime /= fTimeScale; 
			}

			float fAlpha = 0.0f;
			if(fCurrFrameTime != 0.0f)
				fAlpha = fExposureTime / fCurrFrameTime;  
			mCamObjCurr = mCamObjCurr * (1.0f - fAlpha) + mCamObjPrev * fAlpha;  

			Vec3 pVelocity = mCamObjCurr.GetRow(3) - mCamObjPrev.GetRow(3);
			float fCurrSpeed = max( pVelocity.GetLength() - 0.01f, 0.0f);

			pCloakParams.x = pCloakParams.y = fCurrSpeed;
      pCloakParams.z = fLightAmount;
      pCloakParams.w = fCloakBlendAmount;
    }

    sData[0].f[0] = pCloakParams.x;
    sData[0].f[1] = pCloakParams.y;
    sData[0].f[2] = pCloakParams.z;
    sData[0].f[3] = pCloakParams.w;
  }

  void sGetFrozenParams(CD3D9Renderer *r)
  {
    CRenderObject *pObj = r->m_RP.m_pCurObject;
    static int nLastObjFrame=-1;    
    static Vec4 pFrozenParams;

    if ( r->m_RP.m_FrameObject != nLastObjFrame  && pObj)
    {
      nLastObjFrame = r->m_RP.m_FrameObject;

      // Get frost blend amount from material layers
      float fFrostBlendAmount = ((pObj->m_nMaterialLayers&0x000000ff))/255.0f;
      if( r->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_NEAREST] )
        fFrostBlendAmount *= 0.5f;

      float fUseObjSpace = (pObj->m_ObjFlags & FOB_MTLLAYERS_OBJSPACE)? 1.0f : 0.0f;
/*
      if( r->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_SAMPLE4] )
      {
        Vec3 pObjPosWS = pObj->m_II.m_Matrix.GetColumn(3);

        // Get amount of light on cpu - dont want to add more shader permutations - this might be useful for other stuff - expose
        ColorF pLightAmount = ColorF(0.0f, 0.0f, 0.0f, 0.0f);
        for( int nCurrDLight = 0; nCurrDLight < r->m_RP.m_DLights[SRendItem::m_RecurseLevel].Num() ; ++nCurrDLight )
        {
          if ( pObj->m_DynLMMask & (1<<nCurrDLight) )
          {
            CDLight *pDL = r->m_RP.m_DLights[SRendItem::m_RecurseLevel][nCurrDLight];

            // compute attenuation if not sun
            float fAttenuation = 1.0f;
            if( !(pDL->m_Flags & DLF_SUN) )
            {
              float fInvRadius = pDL->m_fRadius;
              if (fInvRadius <= 0)
                fInvRadius = 1.f;

              fInvRadius = 1.f / fInvRadius;

              // light position
              Vec3 pLightVec = pDL->m_Origin - pObjPosWS;

              // compute attenuation
              pLightVec *= fInvRadius;
              fAttenuation = clamp_tpl<float>(1.0f - (pLightVec.x * pLightVec.x + pLightVec.y * pLightVec.y + pLightVec.z * pLightVec.z), 0.0f, 1.0f);
            }

            pLightAmount += fAttenuation * pDL->m_Color;
          }
        }

        // Add ambient
        ColorF &pAmbient = r->m_RP.m_pCurInstanceInfo->m_AmbColor;
        //pLightAmount += pAmbient;

        pFrozenParams.x = pLightAmount[0];
        pFrozenParams.y = pLightAmount[1];
        pFrozenParams.z = pLightAmount[2];
        pFrozenParams.w = fFrostBlendAmount;
      }
      else
*/
      {
        pFrozenParams.x = pFrozenParams.y = pFrozenParams.z = fFrostBlendAmount;
        pFrozenParams.w = fUseObjSpace;
      }
    }

    sData[0].f[0] = pFrozenParams.x;
    sData[0].f[1] = pFrozenParams.y;
    sData[0].f[2] = pFrozenParams.z;
    sData[0].f[3] = pFrozenParams.w;
  }

  void UpdateConstParamsPF( )
  {
    // Per frame - hardcoded/fast - update of commonly used data - feel free to improve this

    if( SCGParamsPF::nFrameID == gRenDev->GetFrameID(true) || SRendItem::m_RecurseLevel!=1 )
      return;

    SCGParamsPF::nFrameID = gRenDev->GetFrameID(true);

    // Updating..

    // ECGP_Matr_PF_ViewProjMatrix
    //mathMatrixTranspose(SCGParamsPF::pCameraProjMatrix.GetData(), gRenDev->m_CameraProjMatrix.GetData(), g_CpuFlags);

    I3DEngine *p3DEngine = gEnv->p3DEngine;
    if (p3DEngine==NULL)
      return;


    // ECGP_PB_WaterLevel - x = static level y = dynamic water ocean/volume level based on camera position, z: dynamic ocean water level
    SCGParamsPF::vWaterLevel = Vec3(p3DEngine->GetWaterLevel(), 
                                    p3DEngine->GetWaterLevel(&gRenDev->GetRCamera().Orig),
                                    p3DEngine->GetOceanWaterLevel(gRenDev->GetRCamera().Orig));

    // ECGP_PB_HDRDynamicMultiplier
    SCGParamsPF::fHDRDynamicMultiplier = p3DEngine->GetHDRDynamicMultiplier();

    // ECGP_PB_VolumetricFogParams
    SCGParamsPF::pVolumetricFogParams = sGetVolumetricFogParams( gcpRendD3D );
    // ECGP_PB_VolumetricFogColor
    SCGParamsPF::pVolumetricFogColor = p3DEngine->GetFogColor();

    Vec4 vTmp;

    const SSkyLightRenderParams *pSkyParams = gRenDev->GetSkyLightRenderParams();
    if( pSkyParams )
    {
      SCGParamsPF::pSkyLightRenderParams = const_cast<SSkyLightRenderParams *>( pSkyParams );

      // ECGP_PB_SkyLightHazeColorPartialMieInScatter
      vTmp = pSkyParams->m_hazeColorMieNoPremul * pSkyParams->m_partialMieInScatteringConst;
      SCGParamsPF::pSkyLightHazeColorPartialMieInScatter = Vec3(vTmp.x, vTmp.y, vTmp.z);

      // ECGP_PB_SkyLightHazeColorPartialRayleighInScatter
      SCGParamsPF::pSkyLightHazeColorPartialRayleighInScatter = pSkyParams->m_hazeColorRayleighNoPremul * pSkyParams->m_partialRayleighInScatteringConst;
    }

    // ECGP_PB_CausticsParams
    vTmp = p3DEngine->GetCausticsParams();
    SCGParamsPF::pCausticsParams = Vec3( vTmp.y, vTmp.z, vTmp.w );

    // ECGP_PF_SunColor
    SCGParamsPF::pSunColor = p3DEngine->GetSunColor();
    // ECGP_PF_SkyColor
    SCGParamsPF::pSkyColor = p3DEngine->GetSkyColor();

    //ECGP_PB_CloudShadingColorSun
    float fSunColorMul, fSkyColorMul;
    p3DEngine->GetCloudShadingMultiplier(fSunColorMul, fSkyColorMul);
    SCGParamsPF::pCloudShadingColorSun = (fSunColorMul * SCGParamsPF::pSunColor); 
    //ECGP_PB_CloudShadingColorSky
    SCGParamsPF::pCloudShadingColorSky =  (fSkyColorMul * SCGParamsPF::pSkyColor);

    // ECGP_PB_SunSkyConstants
    float fMaxSunSky = max(SCGParamsPF::pSunColor.x+SCGParamsPF::pSkyColor.x,max(SCGParamsPF::pSunColor.y+SCGParamsPF::pSkyColor.y,SCGParamsPF::pSunColor.z+SCGParamsPF::pSkyColor.z));
		float fInvMaxSunSky = 1.0f/(fMaxSunSky+0.0001f);		// +small bias to avoid FP exeception

		float fMaxSun = max(SCGParamsPF::pSunColor.x,max(SCGParamsPF::pSunColor.y,SCGParamsPF::pSunColor.z));	

    SCGParamsPF::pSunSkyConstants = Vec4( fInvMaxSunSky * SCGParamsPF::pSkyColor.x,
                                          fInvMaxSunSky * SCGParamsPF::pSkyColor.y,
                                          fInvMaxSunSky * SCGParamsPF::pSkyColor.z,
                                          fMaxSunSky / (fMaxSun+0.0001f) );	// +small bias to avoid FP exeception

		// ECGP_PB_DecalZFightingRemedy
		float *mProj = (float *)gcpRendD3D->m_matProj->GetTop();
		float s = clamp_tpl(CRenderer::CV_r_ZFightingDepthScale, 0.1f, 1.0f);

		SCGParamsPF::pDecalZFightingRemedy.x = s; // scaling factor to pull decal in front
		SCGParamsPF::pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4*3+2]); // correction factor for homogeneous z after scaling is applied to xyzw { = ( 1 - v[0] ) * zMappingRageBias }
		SCGParamsPF::pDecalZFightingRemedy.z = clamp_tpl(CRenderer::CV_r_ZFightingExtrude, 0.0f, 1.0f);

		// alternative way the might save a bit precision
		//SCGParamsPF::pDecalZFightingRemedy.x = s; // scaling factor to pull decal in front
		//SCGParamsPF::pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4*2+2]);
		//SCGParamsPF::pDecalZFightingRemedy.z = clamp_tpl(CRenderer::CV_r_ZFightingExtrude, 0.0f, 1.0f);
  }
}

void CHWShader_D3D::mfCommitParams(bool bSetPM)
{
#ifdef MERGE_SHADER_PARAMETERS
#if defined (DIRECT3D9)
  PROFILE_FRAME(CommitShaderParams);

	if (gcpRendD3D->m_RP.m_PersFlags2 & RBPF2_COMMIT_CM)
	{
		mfSetCM();
		gcpRendD3D->m_RP.m_PersFlags2 &= ~RBPF2_COMMIT_CM;
	}
	if (gcpRendD3D->m_RP.m_PersFlags2 & RBPF2_COMMIT_PF)
	{
		mfSetPF();
		gcpRendD3D->m_RP.m_PersFlags2 &= ~RBPF2_COMMIT_PF;
	}

  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  int i;
  if (m_NumPSParamsToCommit > 0)
  {
    //std::sort(m_PSParamsToCommit, m_PSParamsToCommit+m_NumPSParamsToCommit);

    int nFirst = m_PSParamsToCommit[0];
    int nParams = 1;
    for (i=1; i<m_NumPSParamsToCommit; i++)
    {
      if (m_PSParamsToCommit[i] != m_PSParamsToCommit[i-1]+1)
      {
        dv->SetPixelShaderConstantF(nFirst, &m_CurPSParams[nFirst].x, nParams);
        nFirst = m_PSParamsToCommit[i];
        nParams = 1;
      }
      else
        nParams++;
    }
    dv->SetPixelShaderConstantF(nFirst, &m_CurPSParams[nFirst].x, nParams);
    m_NumPSParamsToCommit = 0;
  }

  if (m_NumVSParamsToCommit > 0)
  {
    //std::sort(m_VSParamsToCommit, m_VSParamsToCommit+m_NumVSParamsToCommit);

    int nFirst = m_VSParamsToCommit[0];
    int nParams = 1;
    for (i=1; i<m_NumVSParamsToCommit; i++)
    {
      if (m_VSParamsToCommit[i] != m_VSParamsToCommit[i-1]+1)
      {
        dv->SetVertexShaderConstantF(nFirst, &m_CurVSParams[nFirst].x, nParams);
        nFirst = m_VSParamsToCommit[i];
        nParams = 1;
      }
      else
        nParams++;
    }
    dv->SetVertexShaderConstantF(nFirst, &m_CurVSParams[nFirst].x, nParams);
    m_NumVSParamsToCommit = 0;
  }
#elif defined(DIRECT3D10)
	#if defined(PS3)
	for (int i=0; i<CB_MAX; i++)
	{
		mfCommitCB(i, eHWSC_Vertex);
		mfCommitCB(i, eHWSC_Pixel);
	}
	#else
	if (gcpRendD3D->m_RP.m_PersFlags2 & (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM))
	{
		gcpRendD3D->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);
		mfSetPF();
		mfSetCM();
	}

  for (int j=0; j<eHWSC_Max; j++)
  {
    for (int i=0; i<CB_MAX; i++)
    {
      mfCommitCB(i, (EHWShaderClass)j);
    }
  }
	#endif
  if (bSetPM)
  {
    SRenderShaderResources *pRes = gcpRendD3D->m_RP.m_pShaderResources;
    if (pRes)
    {
      if (m_pCurInstVS && m_pCurInstVS->m_bHasPMParams)
      {
        SRenderObjData *pOD = gcpRendD3D->m_RP.m_pCurObject->GetObjData();
        if (pOD && pOD->m_Constants.size())
        {
          mfSetCBConst(0, CB_PER_MATERIAL, eHWSC_Vertex, &pOD->m_Constants[0][0], pOD->m_Constants.size()*4, pOD->m_Constants.size());
          mfCommitCB(CB_PER_MATERIAL, eHWSC_Vertex);
        }
        else
        {
          ID3D10Buffer *pCB = (ID3D10Buffer *)pRes->m_pCB[eHWSC_Vertex];
          mfSetCB(eHWSC_Vertex, CB_PER_MATERIAL, pCB);
        }
      }
      if (m_pCurInstPS && m_pCurInstPS->m_bHasPMParams)
      {
        ID3D10Buffer *pCB = (ID3D10Buffer *)pRes->m_pCB[eHWSC_Pixel];
        mfSetCB(eHWSC_Pixel, CB_PER_MATERIAL, pCB);
      }
      if (m_pCurInstGS && m_pCurInstGS->m_bHasPMParams)
      {
        ID3D10Buffer *pCB = (ID3D10Buffer *)pRes->m_pCB[eHWSC_Geometry];
        mfSetCB(eHWSC_Geometry, CB_PER_MATERIAL, pCB);
      }
    }
    if (m_pCurReqCB[eHWSC_Pixel][CB_PER_LIGHT])
      mfSetCB(eHWSC_Pixel, CB_PER_LIGHT, m_pCurReqCB[eHWSC_Pixel][CB_PER_LIGHT]);
    if (m_pCurReqCB[eHWSC_Vertex][CB_PER_LIGHT])
      mfSetCB(eHWSC_Vertex, CB_PER_LIGHT, m_pCurReqCB[eHWSC_Vertex][CB_PER_LIGHT]);
    if (m_pCurReqCB[eHWSC_Geometry][CB_PER_LIGHT])
      mfSetCB(eHWSC_Geometry, CB_PER_LIGHT, m_pCurReqCB[eHWSC_Geometry][CB_PER_LIGHT]);
  }

#elif defined(OPENGL)
  // mfSetVSConst() and mfSetPSConst() will set the constant directly through
  // the IDirect3DDevice9/OpenGL wrapper.
  assert(m_NumPSParamsToCommit == 0);
  assert(m_NumVSParamsToCommit == 0);
#endif
#endif
}


static char *sSH[] = {"VS", "PS", "GS"};
static char *sComp[] = {"x", "y", "z", "w"};


#if defined (DIRECT3D10)
float *CHWShader_D3D::mfSetParameters(SCGParam *pParams, int nINParams, float *pDst, EHWShaderClass eSH, int nMaxVecs)
#else
float *CHWShader_D3D::mfSetParameters(SCGParam *pParams, int nINParams, float *pDst, EHWShaderClass eSH)
#endif
{
  PROFILE_FRAME(CHWShaderSetParams);

  CRenderObject* pObj;
  I3DEngine *pEng;
  CDLight *pDL;
  SLightPass *pLP;
  CRendElement *pRE;
  SEfResTexture *pRT;
  SEnvTexture *pEnv;
  float *pData;
  register float *pSrc;
  Vec3 v;
  uint i;
  const SCGParam *ParamBind;
  Matrix33 rotMat;
  int nParams;

  if (!pParams)
    return pDst;

  CD3D9Renderer *r = gcpRendD3D;

  for (int nParam=0; nParam<nINParams; nParam++)
  {
    ParamBind = &pParams[nParam];
#if defined(OPENGL)
    if (ParamBind->m_Flags & PF_MATRIX)
    {
      assert(ParamBind->m_isMatrix != 0);//must be of a matrix type, otherwise cg reports an error
    }
#endif
    pSrc = &sData[0].f[0];
    nParams = ParamBind->m_nParameters;
    int nCurType = ParamBind->m_eCGParamType;
    for (int nComp=0; nComp<4; nComp++, nCurType>>=8)
    {
#ifdef DO_RENDERLOG
      if (CRenderer::CV_r_log >= 3)
      {
        if (ParamBind->m_Flags & PF_SINGLE_COMP)
          r->Logv(SRendItem::m_RecurseLevel, " Set %s parameter '%s' (%d vectors, reg: %d)\n", sSH[eSH], r->m_cEF.mfGetShaderParamName((ECGParam)(nCurType & 0xff)), nParams, ParamBind->m_dwBind);
        else
          r->Logv(SRendItem::m_RecurseLevel, " Set %s parameter '%s' (%d vectors, reg: %d.%s)\n", sSH[eSH], r->m_cEF.mfGetShaderParamName((ECGParam)(nCurType & 0xff)), nParams, ParamBind->m_dwBind, sComp[nComp]);
      }
#endif
      switch(nCurType & 0xff)
      {
      case ECGP_Matr_PI_ViewProj:
        if (!(r->m_RP.m_ObjFlags & FOB_TRANS_MASK))
          mathMatrixTranspose((float *)&sData[0].f[0], r->m_CameraProjMatrix.GetData(), g_CpuFlags);
        else
        {
          mathMatrixMultiply_Transp2(&sData[4].f[0], r->m_CameraProjMatrix.GetData(), r->m_RP.m_pCurObject->m_II.m_Matrix.GetData(), g_CpuFlags);
          mathMatrixTranspose((float *)&sData[0].f[0], &sData[4].f[0], g_CpuFlags);
        }
        pSrc = &sData[0].f[0];
        break;

      case ECGP_Matr_PF_ViewProjMatrix:
        mathMatrixTranspose((float *)&sData[0].f[0], r->m_CameraProjMatrix.GetData(), g_CpuFlags);
        break;
      case ECGP_Matr_PF_ViewProjZeroMatrix:
        mathMatrixTranspose((float *)&sData[0].f[0], r->m_CameraProjZeroMatrix.GetData(), g_CpuFlags);
        break;
      case ECGP_Matr_PB_ViewProjMatrix_I:
        mathMatrixInverse((float *)&sData[0].f[0], r->m_CameraProjMatrix.GetData(), g_CpuFlags);
        mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);        
        break;
      case ECGP_Matr_PB_ViewProjMatrix_IT:
        mathMatrixInverse((float *)&sData[0].f[0], r->m_CameraProjMatrix.GetData(), g_CpuFlags);
        break;
      case ECGP_Matr_PI_Obj:
        {
          pData = r->m_RP.m_pCurInstanceInfo->m_Matrix.GetData();
#if defined(_CPU_SSE) && !defined(_DEBUG)
          sData[0].m128 = _mm_load_ps(&pData[0]);
          sData[1].m128 = _mm_load_ps(&pData[4]);
          sData[2].m128 = _mm_load_ps(&pData[8]);
#else
          sData[0].f[0] = pData[0]; sData[0].f[1] = pData[1]; sData[0].f[2] = pData[2]; sData[0].f[3] = pData[3];
          sData[1].f[0] = pData[4]; sData[1].f[1] = pData[5]; sData[1].f[2] = pData[6]; sData[1].f[3] = pData[7];
          sData[2].f[0] = pData[8]; sData[2].f[1] = pData[9]; sData[2].f[2] = pData[10]; sData[2].f[3] = pData[11];
#endif
          if (!(r->m_RP.m_PersFlags & RBPF_SHADOWGEN) && (r->m_RP.m_ObjFlags & FOB_TRANS_MASK) && !(r->m_RP.m_ObjFlags & FOB_CAMERA_SPACE))
          {
            sData[0].f[3] -= r->GetRCamera().Orig.x;
            sData[1].f[3] -= r->GetRCamera().Orig.y;
            sData[2].f[3] -= r->GetRCamera().Orig.z;
          }
        }
        break;

      case ECGP_PB_MeshScale:
        sData[0].f[0] = r->m_vMeshScale[0]; sData[0].f[1] = r->m_vMeshScale[1]; sData[0].f[2] = r->m_vMeshScale[2]; sData[0].f[3] = r->m_vMeshScale[3];
        break;

      case ECGP_Matr_PI_Composite:
        {
          mathMatrixMultiply(&sData[0].f[0], (float *)r->m_matProj->GetTop(), (float *)r->m_matView->GetTop(), g_CpuFlags);
          mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        }
        break;

      case ECGP_Matr_PB_ProjMatrix:
        //*(Matrix44 *)(&sData[0].f[0]) = r->m_ProjMatrix;
        mathMatrixTranspose((float *)&sData[0].f[0], r->m_ProjMatrix.GetData(), g_CpuFlags);
        break;
      case ECGP_Matr_PB_UnProjMatrix:
        mathMatrixMultiply(&sData[0].f[0], (float *)r->m_matProj->GetTop(), (float *)r->m_matView->GetTop(), g_CpuFlags);
        mathMatrixInverse(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        break;

      case ECGP_Matr_PB_View_IT:
        mathMatrixMultiply(&sData[0].f[0], r->m_CameraMatrix.GetData(), sTranspose(r->m_RP.m_pCurObject->m_II.m_Matrix), g_CpuFlags);
        mathMatrixInverse(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        break;
      case ECGP_Matr_PB_View:
        mathMatrixMultiply(&sData[0].f[0], r->m_CameraMatrix.GetData(), sTranspose(r->m_RP.m_pCurObject->m_II.m_Matrix), g_CpuFlags);
        mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        break;
      case ECGP_Matr_PB_View_I:
        mathMatrixMultiply(&sData[0].f[0], r->m_CameraMatrix.GetData(), sTranspose(r->m_RP.m_pCurObject->m_II.m_Matrix), g_CpuFlags);
        mathMatrixInverse(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
        break;
      case ECGP_Matr_PB_View_T:
        mathMatrixMultiply(&sData[0].f[0], r->m_CameraMatrix.GetData(), sTranspose(r->m_RP.m_pCurObject->m_II.m_Matrix), g_CpuFlags);
        break;

      case ECGP_Matr_PB_Camera:
        mathMatrixTranspose(&sData[0].f[0], r->m_CameraMatrix.GetData(), g_CpuFlags);                        
        break;
      case ECGP_Matr_PB_Camera_T:
        pSrc = r->m_CameraMatrix.GetData();
        break;
      case ECGP_Matr_PB_Camera_I:        
        mathMatrixInverse(&sData[0].f[0], r->m_CameraMatrix.GetData(), g_CpuFlags);
        mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);        
        break;
      case ECGP_Matr_PB_Camera_IT:
        mathMatrixInverse(&sData[0].f[0], r->m_CameraMatrix.GetData(), g_CpuFlags);
        break;

      case ECGP_Matr_PI_Obj_T:
        *(Matrix44 *)(&sData[0].f[0]) = Matrix44(r->m_RP.m_pCurObject->m_II.m_Matrix);
        break;

      case ECGP_PI_MotionBlurData:        
        sGetMotionBlurData(r);
        break;

      case ECGP_PI_CloakMotionData:        
        sGetCloakMotionData(r);
        break;

      case ECGP_PI_CloakParams:
        sGetCloakParams(r);
        break;

      case ECGP_Matr_PB_LightMatrix:
        pSrc = sGetLightMatrix(r);
        break;
      case ECGP_Matr_PB_TerrainBase:
        pSrc = sGetTerrainBase(r);
        break;
      case ECGP_Matr_PB_TerrainLayerGen:
        pSrc = sGetTerrainLayerGen(r);
        break;
      case ECGP_Matr_PI_TexMatrix:
        pSrc = sGetTexMatrix(r, ParamBind);
        break;
      case ECGP_Matr_PB_Temp4_0:
      case ECGP_Matr_PB_Temp4_1:
      case ECGP_Matr_PB_Temp4_2:
      case ECGP_Matr_PB_Temp4_3:
        pSrc = r->m_TempMatrices[ParamBind->m_eCGParamType-ECGP_Matr_PB_Temp4_0][ParamBind->m_nID].GetData();
        break;
      case ECGP_Matr_PB_2Temp4_0:
      case ECGP_Matr_PB_2Temp4_1:
      case ECGP_Matr_PB_2Temp4_2:
      case ECGP_Matr_PB_2Temp4_3:
        pSrc = r->m_TempMatrices[ParamBind->m_eCGParamType-ECGP_Matr_PB_2Temp4_0][ParamBind->m_nID].GetData();
        break;
      case ECGP_Matr_PI_TCMMatrix:
        pRT = r->m_RP.m_ShaderTexResources[ParamBind->m_nID];
        if (pRT)
          *(Matrix44 *)(&sData[0].f[0]) = pRT->m_TexModificator.m_TexMatrix;
        else
        {
          //assert(0);
          *(Matrix44 *)(&sData[0].f[0]) = sIdentityMatrix;
        }
        break;
      case ECGP_Matr_PI_TCGMatrix:
        pRT = r->m_RP.m_ShaderTexResources[ParamBind->m_nID];
        if (pRT)
          *(Matrix44 *)(&sData[0].f[0]) = pRT->m_TexModificator.m_TexGenMatrix;
        else
        {
          //assert(0);
          *(Matrix44 *)(&sData[0].f[0]) = sIdentityMatrix;
        }
        break;
      case ECGP_PL_LightsPos:
        {
          pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
          for (i=0; i<pLP->nLights; i++)
          {
            pDL = pLP->pLights[i];
            v = pDL->m_Origin - r->GetRCamera().Orig;
            sData[i].f[0] = v.x;
            sData[i].f[1] = v.y;
            sData[i].f[2] = v.z;

            float fRadius = pDL->m_fRadius;
            if (fRadius <= 0)
              fRadius = 1.f;
            sData[i].f[3] = 1.f / fRadius;
          }
        }
        break;
      case ECGP_PL_ShadowMasks:
        pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
        for (i=0; i<pLP->nLights; i++)
        {
          pDL = pLP->pLights[i];
          sData[i].f[0] = pDL->m_ShadowChanMask.x;
          sData[i].f[1] = pDL->m_ShadowChanMask.y;
          sData[i].f[2] = pDL->m_ShadowChanMask.z;
          sData[i].f[3] = pDL->m_ShadowChanMask.w;
        }
        break;
      case ECGP_PB_DiffuseMulti:
        pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
        for (i=0; i<pLP->nLights; i++)
        {
          pDL = pLP->pLights[i];
          sData[i].f[0] = pDL->m_Color[0];
          sData[i].f[1] = pDL->m_Color[1];
          sData[i].f[2] = pDL->m_Color[2];
          sData[i].f[3] = r->m_RP.m_fCurOpacity * r->m_RP.m_pCurObject->m_fAlpha;

          if( r->m_RP.m_pCurObject->m_nMaterialLayers )
            sData[i].f[3] *= sGetMaterialLayersOpacity( r );

          if (r->m_RP.m_pShaderResources)
          {
            sData[i].f[0] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][0];
            sData[i].f[1] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][1];
            sData[i].f[2] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][2];
          }

          if (r->m_RP.m_pShaderResources && (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE))
          {
            sData[i].f[0] *= r->m_RP.m_fCurOpacity;
            sData[i].f[1] *= r->m_RP.m_fCurOpacity;
            sData[i].f[2] *= r->m_RP.m_fCurOpacity;
          }
        }
        break;
      case ECGP_PB_SpecularMulti:
        pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
        for (i=0; i<pLP->nLights; i++)
        {
          pDL = pLP->pLights[i];
          sData[i].f[0] = pDL->m_Color[0] * pDL->m_Color.a;
          sData[i].f[1] = pDL->m_Color[1] * pDL->m_Color.a;
          sData[i].f[2] = pDL->m_Color[2] * pDL->m_Color.a;
          sData[i].f[3] = pDL->m_SpecMult;

          if (r->m_RP.m_pShaderResources)
          {
            sData[i].f[0] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][0];
            sData[i].f[1] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][1];
            sData[i].f[2] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][2];
          }

          if (r->m_RP.m_pShaderResources && (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE))
          {
            sData[i].f[0] *= r->m_RP.m_fCurOpacity;
            sData[i].f[1] *= r->m_RP.m_fCurOpacity;
            sData[i].f[2] *= r->m_RP.m_fCurOpacity;
          }
        }
        break;
      case ECGP_PI_AmbientOpacity:
        pObj = r->m_RP.m_pCurObject;
        sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 1.0f;  

        sData[0].f[0] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[0];
        sData[0].f[1] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[1]; 
        sData[0].f[2] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[2];  
        sData[0].f[3] = r->m_RP.m_fCurOpacity * pObj->m_fAlpha; // object opacity  

        if(  r->m_RP.m_pCurObject->m_nMaterialLayers )
          sData[0].f[3] *= sGetMaterialLayersOpacity( r );

        if (r->m_RP.m_pShaderResources)
        {
          if (r->m_RP.m_nShaderQuality == eSQ_Low)
          {
            sData[0].f[0] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][0];
            sData[0].f[1] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][1];
            sData[0].f[2] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][2];
          }
          sData[0].f[0] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][0];
          sData[0].f[1] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][1];
          sData[0].f[2] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][2];

          if ( r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE )
          {
            sData[0].f[0] *= r->m_RP.m_fCurOpacity;
            sData[0].f[1] *= r->m_RP.m_fCurOpacity;
            sData[0].f[2] *= r->m_RP.m_fCurOpacity;
          }

          if( CRenderer::CV_r_postprocess_effects && CRenderer::CV_r_nightvision )
          {
            // If nightvision active, brighten up ambient
            using namespace NPostEffects;
            static CEffectParam *pParam = PostEffectMgr().GetByName("NightVision_Active"); 
            float fActive( pParam->GetParam() );
         //   float fOffset = (CRenderer::CV_r_hdrrendering) ? 1.25f : 0.25;
            if( fActive )
            { 
              sData[0].f[0] += 0.25f;//fOffset;
              sData[0].f[1] += 0.25f;//fOffset;
              sData[0].f[2] += 0.25f;//fOffset;  
            }
          }
        }
        break;
      case ECGP_PI_Ambient:
        pObj = r->m_RP.m_pCurObject;
        sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 1.0f;  

        sData[0].f[0] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[0];
        sData[0].f[1] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[1]; 
        sData[0].f[2] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[2];  
        sData[0].f[3] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[3];

        if (r->m_RP.m_pShaderResources)
        {
          sData[0].f[0] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][0];
          sData[0].f[1] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][1];
          sData[0].f[2] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][2];
        }

        if (r->m_RP.m_pShaderResources && (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE))
        {
          sData[0].f[0] *= r->m_RP.m_fCurOpacity;
          sData[0].f[1] *= r->m_RP.m_fCurOpacity;
          sData[0].f[2] *= r->m_RP.m_fCurOpacity;
        }  
        break;
      case ECGP_PI_ObjectAmbColComp:
        pObj = r->m_RP.m_pCurObject;
        sData[0].f[0] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[3];
        sData[0].f[1] = r->m_RP.m_fCurOpacity * pObj->m_fAlpha;

        if( r->m_RP.m_pCurObject->m_nMaterialLayers )
          sData[0].f[1] *= sGetMaterialLayersOpacity( r );

        sData[0].f[3] = pObj->m_fRenderQuality;
        sData[0].f[2] = 0.f;
        break;
      case ECGP_PL_LDiffuseColors:
        assert(0);
        break;
      case ECGP_PL_LSpecularColors:
        assert(0);
        break;
      case ECGP_PI_ObjColor:
        sData[0].f[0] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[0];
        sData[0].f[1] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[1];
        sData[0].f[2] = r->m_RP.m_pCurInstanceInfo->m_AmbColor[2];
        sData[0].f[3] = r->m_RP.m_pCurObject->m_fAlpha;
        break;
      case ECGP_PB_LightningPos:
        iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, v);
        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 0.0f;
        break;
      case ECGP_PB_LightningColSize:
        iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, v);
        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;

        iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE, v);
        sData[0].f[3] = v.x * 0.01f;			
        break;
      case ECGP_PI_EnvColor:
        if (!CTexture::m_pCurEnvTexture)
        {
          pEnv = NULL;
          v = r->m_RP.m_pCurObject->GetTranslation();
          pEnv = CTexture::FindSuitableEnvLCMap(v, true, 0, 0);
          if (pEnv)
            CTexture::m_pCurEnvTexture = pEnv;
        }
        if (!CTexture::m_pCurEnvTexture)
        {
          sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 1.0f;
        }
        else
        {
          for (i=0; i<6; i++)
          {
            sData[i].f[0] = CTexture::m_pCurEnvTexture->m_EnvColors[i][0];
            sData[i].f[1] = CTexture::m_pCurEnvTexture->m_EnvColors[i][1];
            sData[i].f[2] = CTexture::m_pCurEnvTexture->m_EnvColors[i][2];
            sData[i].f[3] = CTexture::m_pCurEnvTexture->m_EnvColors[i][3];
          }
        }
        break;
      case ECGP_PB_LightsNum:
        pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
        sData[0].f[nComp] = (float)pLP->nLights;
        break;
      case ECGP_PB_WaterLevel:
        sData[0].f[0] = SCGParamsPF::vWaterLevel.x;
        sData[0].f[1] = SCGParamsPF::vWaterLevel.y;
        sData[0].f[2] = SCGParamsPF::vWaterLevel.z;
        sData[0].f[3] = 1.0f;
        break;
      case ECGP_PI_Wind:
        sGetWind(r);
        break;
      case ECGP_PB_HDRDynamicMultiplier:
        sData[0].f[nComp] = SCGParamsPF::fHDRDynamicMultiplier;
        break;
      case ECGP_PB_FromRE:
        pRE = r->m_RP.m_pRE;
        if (!pRE || !(pData=(float *)pRE->m_CustomData))
          sData[0].f[nComp] = 0;
        else
          sData[0].f[nComp] = pData[(ParamBind->m_nID>>(nComp*8))&0xff];
        break;
      case ECGP_PI_FromObject:
        if (r->m_RP.m_pCurObject->m_CustomData)
        {
          pData = (float *)r->m_RP.m_pCurObject->m_CustomData;
          sData[0].f[nComp] = pData[(ParamBind->m_nID>>(nComp*8))&0xff];
        }
        else
          sData[0].f[nComp] = 0;
        break;
      case ECGP_PB_ObjVal:
        pData = (float *)r->m_RP.m_pCurObject->m_fTempVars;
        sData[0].f[nComp] = pData[(ParamBind->m_nID>>(nComp*8))&0xff];
        break;
      case ECGP_PI_OSCameraPos:
        {
          sTranspose(r->m_RP.m_pCurObject->m_II.m_Matrix, (Matrix44*)&sData[4].f[0]);
          mathMatrixInverse(&sData[0].f[0], &sData[4].f[0], g_CpuFlags);
          TransformPosition(v, r->GetRCamera().Orig, (Matrix44&)sData);
          sData[0].f[0] = v.x;
          sData[0].f[1] = v.y;
          sData[0].f[2] = v.z;
          sData[0].f[3] = 1.f;
        }
        break;
      case ECGP_PI_GeomCenter:
        if (r->m_RP.m_pRE)
        {
          r->m_RP.m_pRE->mfCenter(v, r->m_RP.m_pCurObject);
          sData[0].f[0] = v.x;
          sData[0].f[1] = v.y;
          sData[0].f[2] = v.z;
          sData[0].f[3] = 0;
        }
        else
        {
          sData[0].f[0] = 0;
          sData[0].f[1] = 0;
          sData[0].f[2] = 0;
          sData[0].f[3] = 0;
        }
        break;
      case ECGP_PB_RotGridScreenOff:
        sGetRotGridScreenOff(r);
        break;
      case ECGP_PB_OutdoorAOParams:
        {
          float * pObjTmpVars = r->m_RP.m_pCurObject->m_fTempVars;
          sData[0].f[0] = iSystem->GetI3DEngine()->GetSkyBrightness();
          sData[0].f[1] = pObjTmpVars[2]; // outdoor AO texgen scale
          sData[0].f[2] = pObjTmpVars[4]-pObjTmpVars[3]; // z range
          sData[0].f[3] = 0.f;//unused
        }
        break;
#ifdef USE_PER_MATERIAL_PARAMS
      case ECGP_PM_Tweakable:
      case ECGP_PM_MatDiffuseColor:
      case ECGP_PM_MatSpecularColor:
        if (r->m_RP.m_pShaderResources)
        {
# ifdef DIRECT3D10
          assert(0);
# else
          Vec4 *pConsts = (Vec4 *)&r->m_RP.m_pShaderResources->m_Constants[eSH][0];
          SRenderObjData *pD = r->m_RP.m_pCurObject->GetObjData();
          if (pD && pD->m_Constants.size())
            pConsts = &pD->m_Constants[0];
          pConsts -= FIRST_REG_PM[eSH];
          i = ParamBind->m_dwBind;
          sData[0].f[0] = pConsts[i][0];
          sData[0].f[1] = pConsts[i][1];
          sData[0].f[2] = pConsts[i][2];
          sData[0].f[3] = pConsts[i][3];

          if ( (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE) )
          {
            sData[0].f[0] *= r->m_RP.m_fCurOpacity;
            sData[0].f[1] *= r->m_RP.m_fCurOpacity;
            sData[0].f[2] *= r->m_RP.m_fCurOpacity;
          }

# endif
        }
        else
        {
          assert(0);
        }
        break;

      case ECGP_PM_MatEmissiveColor:
        if (r->m_RP.m_pShaderResources)
        {
# ifdef DIRECT3D10
          assert(0);
# else
          Vec4 *pConsts = (Vec4 *)&r->m_RP.m_pShaderResources->m_Constants[eSH][0];
          pConsts -= FIRST_REG_PM[eSH];
          i = ParamBind->m_dwBind;
          sData[0].f[0] = pConsts[i][0];
          sData[0].f[1] = pConsts[i][1];
          sData[0].f[2] = pConsts[i][2];
          sData[0].f[3] = pConsts[i][3];

          if ( (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE) )
          {
            sData[0].f[0] *= r->m_RP.m_fCurOpacity;
            sData[0].f[1] *= r->m_RP.m_fCurOpacity;
            sData[0].f[2] *= r->m_RP.m_fCurOpacity;
          }

          if( CRenderer::CV_r_postprocess_effects && CRenderer::CV_r_nightvision )
          {
            // If nightvision active, brighten up ambient
            using namespace NPostEffects;
            static CEffectParam *pParam = PostEffectMgr().GetByName("NightVision_Active"); 
            float fActive( pParam->GetParam() );
            //float fOffset = (CRenderer::CV_r_hdrrendering) ? 2.5f : 0.25;
            if( fActive )
            { 
              sData[0].f[0] += 0.25f;//fOffset;
              sData[0].f[1] += 0.25f;//fOffset;
              sData[0].f[2] += 0.25f;//fOffset;  
            }
          }
# endif
        }
        else
        {
          assert(0);
        }
        break;
#else
      case ECGP_PM_Tweakable:
        assert(ParamBind->m_pData);
        if (ParamBind->m_pData)
        {
          bool bResult = ParamBind->GetTweakable(&sData[0].f[0], nComp);
          assert(bResult == true);
          if (!bResult)
            sData[0].f[nComp] = ParamBind->m_pData->d.fData[nComp];
        }
        break;
      case ECGP_PM_MatDiffuseColor:
        sData[0].f[0] = 1.0f;
        sData[0].f[1] = 1.0f;
        sData[0].f[2] = 1.0f;
        sData[0].f[3] = 1.0f;
        if (pLM=r->m_RP.m_pCurLightMaterial)
        {
          sData[0].f[0] = pLM->m_Diffuse[0];
          sData[0].f[1] = pLM->m_Diffuse[1]; 
          sData[0].f[2] = pLM->m_Diffuse[2];
          sData[0].f[3] = pLM->m_Diffuse[3];
        }

        if ( (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE) )
        {
          sData[0].f[0] *= r->m_RP.m_fCurOpacity;
          sData[0].f[1] *= r->m_RP.m_fCurOpacity;
          sData[0].f[2] *= r->m_RP.m_fCurOpacity;
        }

        break;
      case ECGP_PM_MatSpecularColor:
        sData[0].f[0] = 1.0f;
        sData[0].f[1] = 1.0f;
        sData[0].f[2] = 1.0f;
        sData[0].f[3] = 8.0f;
        if (pLM=r->m_RP.m_pCurLightMaterial)
        {
          sData[0].f[0] = pLM->m_Specular[0];
          sData[0].f[1] = pLM->m_Specular[1];
          sData[0].f[2] = pLM->m_Specular[2];
          sData[0].f[3] = max(r->m_RP.m_pCurLightMaterial->m_SpecShininess, 0.1f);
        }

        if ( (r->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE) )
        {
          sData[0].f[0] *= r->m_RP.m_fCurOpacity;
          sData[0].f[1] *= r->m_RP.m_fCurOpacity;
          sData[0].f[2] *= r->m_RP.m_fCurOpacity;
        }
        break;
      case ECGP_PM_MatReflectColor:
        sData[0].f[0] = 1.0f;
        sData[0].f[1] = 1.0f;
        sData[0].f[2] = 1.0f;
        sData[0].f[3] = 1.0f;

        if( pLM = r->m_RP.m_pCurLightMaterial )
        {
          sData[0].f[0] = pLM->m_Specular[0];
          sData[0].f[1] = pLM->m_Specular[1];
          sData[0].f[2] = pLM->m_Specular[2];

          bool bRealtimeCm = false;
          if( r->m_RP.m_pShaderResources )
          {          
            SEfResTexture *pTexEnv = r->m_RP.m_pShaderResources->m_Textures[EFTT_ENV];
            if( pTexEnv )
              bRealtimeCm = ( pTexEnv->m_Sampler.m_eTexType == eTT_AutoCube );
          }

          // Remove diffuse color - in shader side we accumulate shading in non real-time cube map version
          if( !bRealtimeCm )
          { 
            if( pLM->m_Diffuse[0] )
              sData[0].f[0] /= pLM->m_Diffuse[0];

            if( pLM->m_Diffuse[1] )
              sData[0].f[1] /= pLM->m_Diffuse[1];

            if( pLM->m_Diffuse[2] )
              sData[0].f[2] /= pLM->m_Diffuse[2];
          }
        }
        break;
      case ECGP_PM_MatEmissiveColor:
        sData[0].f[0] = 0;
        sData[0].f[1] = 0;
        sData[0].f[2] = 0;
        sData[0].f[3] = 1;
        if (pLM=r->m_RP.m_pCurLightMaterial)
        {
          sData[0].f[0] += pLM->m_Emission[0] * r->m_RP.m_fCurOpacity;
          sData[0].f[1] += pLM->m_Emission[1] * r->m_RP.m_fCurOpacity;
          sData[0].f[2] += pLM->m_Emission[2] * r->m_RP.m_fCurOpacity;

          if( CRenderer::CV_r_postprocess_effects && CRenderer::CV_r_nightvision )
          {
            // If nightvision active, brighten up ambient
            using namespace NPostEffects;
            static CEffectParam *pParam = PostEffectMgr().GetByName("NightVision_Active"); 
            float fActive( pParam->GetParam() );
            if( fActive )
            {
              sData[0].f[0] += 0.25f;//0.75f;
              sData[0].f[1] += 0.25f;//0.75f;
              sData[0].f[2] += 0.25f;//0.75f;  
            }
          }

        }
        break;
#endif
      case ECGP_PB_SunSkyConstants:
        sData[0].f[0] = SCGParamsPF::pSunSkyConstants.x;
        sData[0].f[1] = SCGParamsPF::pSunSkyConstants.y;
        sData[0].f[2] = SCGParamsPF::pSunSkyConstants.z;
        sData[0].f[3] = SCGParamsPF::pSunSkyConstants.w;
        break;

      case ECGP_PI_RAM_HQAmbientCube:
        {
          pRE = r->m_RP.m_pRE;
          assert(pRE->mfGetType() == eDATA_Mesh);
          IRenderMesh* pRM = ((CREMesh *)pRE)->m_pRenderMesh;

          if(pRM)
          {
            AABB Box;
            pRM->GetBBox(Box.min,Box.max);
            Box.SetTransformedAABB(r->m_RP.m_pCurObject->m_II.m_Matrix,Box);
            IVisArea* pVisArea	=	r->m_RP.m_pCurObject->m_pVisArea;
            if(pVisArea)
              pVisArea->CalcHQAmbientCube(&sData[0].f[0],Box.min,Box.max);
            else
            {
              Vec3 cSky;
              iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_COLOR,cSky);
              Vec3&	rACube0	=	*reinterpret_cast<Vec3*>(&sData[0].f[0]);
              Vec3&	rACube1	=	*reinterpret_cast<Vec3*>(&sData[1].f[0]);
              Vec3&	rACube2	=	*reinterpret_cast<Vec3*>(&sData[2].f[0]);
              Vec3&	rACube3	=	*reinterpret_cast<Vec3*>(&sData[3].f[0]);
              Vec3&	rACube4	=	*reinterpret_cast<Vec3*>(&sData[4].f[0]);
              Vec3&	rACube5	=	*reinterpret_cast<Vec3*>(&sData[5].f[0]);
              rACube0	=	rACube1	=	rACube2	=	rACube3	=	rACube4	=	rACube5	=	cSky;
              sData[6].f[0]=sData[6].f[1]=sData[6].f[2]=sData[6].f[3]=
                sData[7].f[0]=sData[7].f[1]=sData[7].f[2]=sData[7].f[3]=1.f;
              sData[8].f[0]=sData[8].f[1]=sData[8].f[2]=sData[8].f[3]=
                sData[9].f[0]=sData[9].f[1]=sData[9].f[2]=sData[9].f[3]=0.95f;
              sData[10].f[0]=sData[10].f[1]=sData[10].f[2]=sData[10].f[3]=1.05f;
              sData[11].f[0]=sData[11].f[1]=sData[11].f[2]=sData[11].f[3]=0.5f;
            }
          }
          if(r->m_RP.m_pShaderResources)
          {
            for(i=0;i<6;i++)
            {
              sData[i].f[0] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][0];
              sData[i].f[1] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][1];
              sData[i].f[2] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][2];

              sData[i].f[0] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][0];
              sData[i].f[1] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][1];
              sData[i].f[2] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][2];
            }
          }
        }
        break;
      case ECGP_PI_RAM_AmbientCube:
        {
          AABB Box;
          pRE = r->m_RP.m_pRE;
          assert(pRE->mfGetType() == eDATA_Mesh);
          IRenderMesh* pRM = ((CREMesh *)pRE)->m_pRenderMesh;
          if (pRM)
          {
            pRM->GetBBox(Box.min,Box.max);
            Box.SetTransformedAABB(r->m_RP.m_pCurObject->m_II.m_Matrix,Box);
            IVisArea* pVisArea	=	r->m_RP.m_pCurObject->m_pVisArea;
            if(pVisArea)
              pVisArea->CalcAmbientCube(&sData[0].f[0],Box.min,Box.max);
            if(r->m_RP.m_pShaderResources)
            {
              for(i=0;i<6;i++)
              {
                sData[i].f[0] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][0];
                sData[i].f[1] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][1];
                sData[i].f[2] *= r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][2];

                sData[i].f[0] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][0];
                sData[i].f[1] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][1];
                sData[i].f[2] += r->m_RP.m_pShaderResources->m_Constants[eHWSC_Pixel][PS_EMISSIVE_COL][2];
              }
            }
          }
        }
        break;
      case ECGP_PI_RAM_ToAABBSpaceScale:
        {
          pRE = r->m_RP.m_pRE;
          assert(pRE->mfGetType() == eDATA_Mesh);
          IRenderMesh* pRM = ((CREMesh *)pRE)->m_pRenderMesh;
          if (pRM)
          {
            AABB Box;
            pRM->GetBBox(Box.min,Box.max);
            Box.SetTransformedAABB(r->m_RP.m_pCurObject->m_II.m_Matrix,Box);
            sData[0].f[0]	=	1.f/max(Box.max.x-Box.min.x,FLT_EPSILON);
            sData[0].f[1]	=	1.f/max(Box.max.y-Box.min.y,FLT_EPSILON);
            sData[0].f[2]	=	1.f/max(Box.max.z-Box.min.z,FLT_EPSILON);
            sData[0].f[3]	=	0;
          }
        }
        break;
      case ECGP_PI_RAM_ToAABBSpaceTranslate:
        {
          pRE = r->m_RP.m_pRE;
          assert(pRE->mfGetType() == eDATA_Mesh);
          IRenderMesh* pRM = ((CREMesh *)pRE)->m_pRenderMesh;
          if (pRM)
          {
            AABB Box;
            pRM->GetBBox(Box.min,Box.max);
            Box.SetTransformedAABB(r->m_RP.m_pCurObject->m_II.m_Matrix,Box);
            sData[0].f[0]	=	-Box.min.x/max(Box.max.x-Box.min.x,FLT_EPSILON);
            sData[0].f[1]	=	-Box.min.y/max(Box.max.y-Box.min.y,FLT_EPSILON);
            sData[0].f[2]	=	-Box.min.z/max(Box.max.z-Box.min.z,FLT_EPSILON);
            sData[0].f[3]	=	0;
          } 
        }
        break;
      case ECGP_PB_GlowParams:
        // to be merged with glow color/diffuse/emissive in future
        if (r->m_RP.m_pShaderResources)
        {
          sData[0].f[0] = r->m_RP.m_pShaderResources->Glow(); 
          sData[0].f[1] = sData[0].f[0];
          sData[0].f[2] = sData[0].f[0];
          sData[0].f[3] = 1.0f;
        }
        else
          sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0.f;
        break;
      case ECGP_PI_VisionParams:    
        {
          CRenderObject *pObj = r->m_RP.m_pCurObject;
          if( pObj )
          {
            float fRecip = 1.0f / 255.0f;
            sData[0].f[0] = float((pObj->m_nVisionParams&0xff000000)>>24) * fRecip;
            sData[0].f[1] = float((pObj->m_nVisionParams&0x00ff0000)>>16) * fRecip;
            sData[0].f[2] = float((pObj->m_nVisionParams&0x0000ff00)>>8 ) * fRecip;
            sData[0].f[3] = float((pObj->m_nVisionParams&0x000000ff) ) * fRecip;
          }
          else
          {
            // pass default color
            sData[0].f[0] = 1.0f;
            sData[0].f[1] = 0.5f;
            sData[0].f[2] = 0.0f;
            sData[0].f[3] = 1.0f;
          }
        }
        break;
      case ECGP_PB_IrregKernel:
        sGetIrregKernel(r);
        break;
      case ECGP_PB_RegularKernel:
        sGetRegularKernel(r);
        break;
      case ECGP_PI_BendInfo:  
        sGetBendInfo(r);
        break;
      case ECGP_PB_DeformWaveX:
        {
          if (r->m_RP.m_pShaderResources && r->m_RP.m_pShaderResources->m_pDeformInfo)
          {
            SDeformInfo *di = r->m_RP.m_pShaderResources->m_pDeformInfo;
            if (di->m_fDividerX != 0)
            {
              sData[0].f[0] = r->m_RP.m_RealTime*di->m_WaveX.m_Freq+di->m_WaveX.m_Phase;
              sData[0].f[1] = di->m_WaveX.m_Amp;
              sData[0].f[2] = di->m_WaveX.m_Level;
              sData[0].f[3] = 1.0f / di->m_fDividerX;
            }
            else
            {
              sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
              sData[0].f[3] = 1.0f;
            }
          }
          else
          {
            sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
            sData[0].f[3] = 1.0f;
          }
        }
        break;
      case ECGP_PB_DeformWaveY:
        {
          if (r->m_RP.m_pShaderResources && r->m_RP.m_pShaderResources->m_pDeformInfo)
          {
            SDeformInfo *di = r->m_RP.m_pShaderResources->m_pDeformInfo;
            if (di->m_fDividerY != 0)
            {
              sData[0].f[0] = r->m_RP.m_RealTime*di->m_WaveY.m_Freq+di->m_WaveY.m_Phase;
              sData[0].f[1] = di->m_WaveY.m_Amp;
              sData[0].f[2] = di->m_WaveY.m_Level;
              sData[0].f[3] = 1.0f / di->m_fDividerY;
            }
            else
            {
              sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
              sData[0].f[3] = 1.0f;
            }
          }
          else
          {
            sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
            sData[0].f[3] = 1.0f;
          }
        }
        break;
      case ECGP_PB_DeformBend:
        if (r->m_RP.m_pShaderResources && r->m_RP.m_pShaderResources->m_pDeformInfo)
        {
          SDeformInfo *di = r->m_RP.m_pShaderResources->m_pDeformInfo;
          sData[0].f[0] = CShaderMan::EvalWaveForm(&di->m_WaveX);
          sData[0].f[1] = CShaderMan::EvalWaveForm(&di->m_WaveY);
          sData[0].f[2] = di->m_fDividerX;
          sData[0].f[3] = di->m_fDividerY;
        }
        else
        {
          sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
          sData[0].f[3] = 1.0f;
        }
        break;
      case ECGP_PB_DeformNoiseInfo:
        if (r->m_RP.m_pShaderResources && r->m_RP.m_pShaderResources->m_pDeformInfo)
        {
          SDeformInfo *di = r->m_RP.m_pShaderResources->m_pDeformInfo;
          sData[0].f[0] = di->m_vNoiseScale[0] * r->m_RP.m_RealTime;
          sData[0].f[1] = di->m_vNoiseScale[1] * r->m_RP.m_RealTime;
          sData[0].f[2] = di->m_vNoiseScale[2] * r->m_RP.m_RealTime;
          sData[0].f[3] = 1.0f;
        }
        else
        {
          sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
          sData[0].f[3] = 1.0f;
        }
        break;
      case ECGP_PB_ShadowOutputChannelMask:
        sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
        sData[0].f[r->m_RP.m_nCurLightChan] = 1;
        break;
      case ECGP_PB_TFactor:
        sData[0].f[0] = r->m_RP.m_CurGlobalColor[0];
        sData[0].f[1] = r->m_RP.m_CurGlobalColor[1];
        sData[0].f[2] = r->m_RP.m_CurGlobalColor[2];
        sData[0].f[3] = r->m_RP.m_CurGlobalColor[3];
        break;
      case ECGP_PI_AlphaTest:
        sData[0].f[0] = 0;
        sData[0].f[1] = 0;
        sData[0].f[2] = 0;
        sData[0].f[3] = r->m_RP.m_pShaderResources ? r->m_RP.m_pShaderResources->m_AlphaRef : 0;

        // specific condition for hair zpass
        if ( r->m_RP.m_pShader->m_Flags2 & EF2_HAIR && !(r->m_RP.m_PersFlags & RBPF_SHADOWGEN))
          sData[0].f[3] = 0.51f;

        if (r->m_RP.m_pCurObject->m_AlphaRef)
          sData[0].f[3] += (1.f/255.f)*r->m_RP.m_pCurObject->m_AlphaRef;

        break;
      case ECGP_PI_MaterialLayersParams:
        {
          CRenderObject *pObj = r->m_RP.m_pCurObject;
          if( pObj )
          {
            sData[0].f[0] = ((pObj->m_nMaterialLayers&0x000000ff))/255.0f;
            sData[0].f[1] = ((pObj->m_nMaterialLayers&0x00ff0000)>>16)/255.0f;
            // Apply attenuation
            sData[0].f[1] *= 1.0f - min(1.0f, pObj->m_fDistance / CRenderer::CV_r_rain_maxviewdist);

            sData[0].f[2] = ((pObj->m_nMaterialLayers&0x0000ff00)>>8)/255.0f;

            sData[0].f[3] = (pObj->m_ObjFlags & FOB_MTLLAYERS_OBJSPACE)? 1.0f : 0.0f;
          }
          else
          {
            sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
          }
        }
        break;

      case ECGP_PI_FrozenLayerParams:
        sGetFrozenParams(r);
        break;

      case ECGP_PB_RandomParams:
        {
          sData[0].f[0] = Random();
          sData[0].f[1] = Random();
          sData[0].f[2] = Random();
          sData[0].f[3] = Random();
        }
        break;
      case ECGP_PI_HMAGradients:
        {
          pRE = r->m_RP.m_pRE;
          assert(pRE->mfGetType() == eDATA_Mesh);
          IRenderMesh* pRM = ((CREMesh *)pRE)->m_pRenderMesh;
          if (pRM && r->m_RP.m_pCurObject)
          {
            AABB Box;
            pRM->GetBBox(Box.min,Box.max);

            const float RANGE	=	static_cast<float>((1<<2)-1)+0.5f;//0.5for rounding
            const unsigned int HMAIndex	=	r->m_RP.m_pCurObject->m_HMAData;
            const float HMARange				=	*reinterpret_cast<float*>(&r->m_RP.m_pCurObject->m_HMAData)/RANGE;

            const float H0	=	static_cast<float>(HMAIndex&((1<<3)-1))*HMARange-RANGE*HMARange;
            const float H1	=	static_cast<float>((HMAIndex>>3)&((1<<3)-1))*HMARange-RANGE*HMARange;
            const float H2	=	static_cast<float>((HMAIndex>>6)&((1<<3)-1))*HMARange-RANGE*HMARange;
            const float H3	=	static_cast<float>((HMAIndex>>9)&((1<<3)-1))*HMARange-RANGE*HMARange;

						const float DeltaX	=	Box.max.x-Box.min.x;
						const float DeltaY	=	Box.max.y-Box.min.y;
						if(fabs(DeltaX)>FLT_EPSILON && fabs(DeltaY)>FLT_EPSILON)
						{
							sData[0].f[0] = (H0-H1)/DeltaX;
							sData[0].f[1] = (H2-H3)/DeltaY;
							sData[0].f[2] = ((H0+H1)*0.5f)/(DeltaX*DeltaX*0.25f);
							sData[0].f[3] = ((H2+H3)*0.5f)/(DeltaY*DeltaY*0.25f);
						}
						else
						{
							sData[0].f[0]=sData[0].f[1]=sData[0].f[2]=sData[0].f[3]=0.f;
						}
          }
        }
        break;
      case ECGP_PB_TempData:
        sData[0].f[0] = r->m_cEF.m_TempVecs[ParamBind->m_nID].x;
        sData[0].f[1] = r->m_cEF.m_TempVecs[ParamBind->m_nID].y;
        sData[0].f[2] = r->m_cEF.m_TempVecs[ParamBind->m_nID].z;
        sData[0].f[3] = r->m_cEF.m_TempVecs[ParamBind->m_nID].w;
        break;
      case ECGP_Matr_SG_ShadowProj_0:
        pSrc = r->m_TempMatrices[0][0].GetData();
        break;
      case ECGP_Matr_SG_ShadowProj_1:
        pSrc = r->m_TempMatrices[1][0].GetData();
        break;
      case ECGP_Matr_SG_ShadowProj_2:
        pSrc = r->m_TempMatrices[2][0].GetData();
        break;
      case ECGP_Matr_SG_ShadowProj_3:
        pSrc = r->m_TempMatrices[3][0].GetData();
        break;
      case ECGP_SG_FrustrumInfo:
        sData[0].f[0] = r->m_RP.m_vFrustumInfo.x;
        sData[0].f[1] = r->m_RP.m_vFrustumInfo.y;
        sData[0].f[2] = r->m_RP.m_vFrustumInfo.z;
        sData[0].f[3] = r->m_RP.m_vFrustumInfo.w;
        break;
      case ECGP_PB_DecalZFightingRemedy:
        sData[0].f[0] = SCGParamsPF::pDecalZFightingRemedy.x;
        sData[0].f[1] = SCGParamsPF::pDecalZFightingRemedy.y;
        sData[0].f[2] = SCGParamsPF::pDecalZFightingRemedy.z;
        sData[0].f[3] = 0;
        break;
      case ECGP_PB_VolumetricFogParams:
        sData[0].f[0] = SCGParamsPF::pVolumetricFogParams.x;
        sData[0].f[1] = SCGParamsPF::pVolumetricFogParams.y;
        sData[0].f[2] = SCGParamsPF::pVolumetricFogParams.z;
        sData[0].f[3] = SCGParamsPF::pVolumetricFogParams.w;
        break;
      case ECGP_PB_VolumetricFogColor:
        sData[0].f[0] = SCGParamsPF::pVolumetricFogColor.x;
        sData[0].f[1] = SCGParamsPF::pVolumetricFogColor.y;
        sData[0].f[2] = SCGParamsPF::pVolumetricFogColor.z;
        sData[0].f[3] = 0.0f;
        break;
      case ECGP_PB_CameraFront:
        v = r->GetRCamera().Z;
        v.Normalize();

        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 0;
        break;
      case ECGP_PB_CameraRight:
        v = r->GetRCamera().X;
        v.Normalize();

        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 0;
        break;
      case ECGP_PB_CameraUp:
        v = r->GetRCamera().Y;
        v.Normalize();

        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 0;
        break;
      case ECGP_PB_RTRect:
        sData[0].f[0] = r->m_cEF.m_RTRect.x;
        sData[0].f[1] = r->m_cEF.m_RTRect.y;
        sData[0].f[2] = r->m_cEF.m_RTRect.z;
        sData[0].f[3] = r->m_cEF.m_RTRect.w;
        break;
      case ECGP_PI_AvgFogVolumeContrib:
        {
          pObj = r->m_RP.m_pCurObject;

          if(pObj)
          {
            if(pObj->m_FogVolumeContribIdx == (uint16) -1)
            {
              pEng = iSystem->GetI3DEngine();
              ColorF newContrib;

              Vec3 vObjPosWS = pObj->GetTranslation();
              if (r->m_RP.m_ObjFlags & FOB_CAMERA_SPACE)
                vObjPosWS += r->GetRCamera().Orig;

              pEng->TraceFogVolumes(vObjPosWS, newContrib);

              pObj->m_FogVolumeContribIdx = r->PushFogVolumeContribution(newContrib);
            }

            const ColorF& contrib(r->GetFogVolumeContribution(pObj->m_FogVolumeContribIdx));
            
            // Pre-multiply alpha (saves 1 instruction in pixel shader)
            if ( !r->m_RP.m_bNotFirstPass )
            {
              sData[0].f[0] = contrib.r * (1 - contrib.a);   
              sData[0].f[1] = contrib.g * (1 - contrib.a);
              sData[0].f[2] = contrib.b * (1 - contrib.a);
            }
            else
            {
              sData[0].f[0] = 0;
              sData[0].f[1] = 0;
              sData[0].f[2] = 0;
            }

            sData[0].f[3] = contrib.a;
          }
          else
            sData[0].f[1] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
        }
        break;
      case ECGP_PB_SkyLightHazeColorPartialMieInScatter:
        {
          if( SCGParamsPF::pSkyLightRenderParams )
          {
            sData[0].f[0] = SCGParamsPF::pSkyLightHazeColorPartialMieInScatter.x;
            sData[0].f[1] = SCGParamsPF::pSkyLightHazeColorPartialMieInScatter.y;
            sData[0].f[2] = SCGParamsPF::pSkyLightHazeColorPartialMieInScatter.z;
            sData[0].f[3] = 0;
          }
          else
          {
            //assert( !"Some shader refers to currently not available sky light constants!" );
            sData[0].f[1] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
          }
        }
        break;
      case ECGP_PB_SkyLightHazeColorPartialRayleighInScatter:
        {
          if( SCGParamsPF::pSkyLightRenderParams )
          {
            sData[0].f[0] = SCGParamsPF::pSkyLightHazeColorPartialRayleighInScatter.x;
            sData[0].f[1] = SCGParamsPF::pSkyLightHazeColorPartialRayleighInScatter.y;
            sData[0].f[2] = SCGParamsPF::pSkyLightHazeColorPartialRayleighInScatter.z;
            sData[0].f[3] = 0;
          }
          else
          {
            //assert( !"Some shader refers to currently not available sky light constants!" );
            sData[0].f[1] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
          }
        }
        break;
      case ECGP_PB_SkyLightSunDirection:
        {
          const SSkyLightRenderParams* pParams(r->GetSkyLightRenderParams());
          if(pParams)
          {
            sData[0].f[0] = pParams->m_sunDirection.x;
            sData[0].f[1] = pParams->m_sunDirection.y;
            sData[0].f[2] = pParams->m_sunDirection.z;
            sData[0].f[3] = 0;
          }
          else
          {
            //assert( !"Some shader refers to currently not available sky light constants!" );
            sData[0].f[1] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
          }
        }
        break;
      case ECGP_PB_SkyLightPhaseFunctionConstants:
        {
          const SSkyLightRenderParams* pParams(r->GetSkyLightRenderParams());
          if(pParams)
          {
            sData[0].f[0] = pParams->m_phaseFunctionConsts.x;
            sData[0].f[1] = pParams->m_phaseFunctionConsts.y;
            sData[0].f[2] = pParams->m_phaseFunctionConsts.z;
            sData[0].f[3] = pParams->m_phaseFunctionConsts.w;
          }
          else
          {
            //assert( !"Some shader refers to currently not available sky light constants!" );
            sData[0].f[1] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
          }
        }
        break;
      case ECGP_PB_LightInfoTC:
        {
          pLP = &r->m_RP.m_LPasses[r->m_RP.m_nCurLightPass];
          if (pLP->nLights)
          {
            int nGroup = pLP->pLights[0]->m_Id >> 2;
            sData[0].f[1] = (float)nGroup / 8;

            // Fast lookup to pre-build table
            int nID = 0;
            for (i=0; i<pLP->nLights; i++)
            {
              pDL = pLP->pLights[i];
              assert ((pDL->m_Id >> 2) == nGroup);
              nID |= (pDL->m_Id&3) << (i*2);
            }
            Vec4 &Data = r->m_RP.m_LightInfo[nID];
            sData[0].f[0] = Data[0];
            sData[0].f[2] = Data[2];
            sData[0].f[3] = Data[3];
            assert(sData[0].f[0] >= 0.0f);
          }
        }
        break;
      case ECGP_PI_Opacity:
        {
          sData[0].f[nComp] = r->m_RP.m_fCurOpacity * r->m_RP.m_pCurObject->m_fAlpha;
        }
        break;
      case ECGP_PB_ResourcesOpacity:
        if (r->m_RP.m_pShaderResources)
        {
          sData[0].f[0] = r->m_RP.m_pShaderResources->Opacity();
          sData[0].f[1] = sData[0].f[0];
          sData[0].f[2] = sData[0].f[0];
          sData[0].f[3] = sData[0].f[0];
        }
        else
          sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0.f;
        break;
      case ECGP_PI_NumInstructions:
        {
          sData[0].f[nComp] = r->m_RP.m_NumShaderInstructions / CRenderer::CV_r_measureoverdrawscale / 256.0f;
        }
        break;
      case ECGP_PB_GlobalShaderFlag:
        assert(ParamBind->m_pData);
        if (ParamBind->m_pData)
        {
          if (!r->m_RP.m_pShader)
            pData = NULL;
          else
          {
            bool bVal = (r->m_RP.m_pShader->m_nMaskGenFX & ParamBind->m_pData->d.nData32[nComp]) != 0;
            sData[0].f[nComp] = (float)(bVal);
          }
        }
        break;
      case ECGP_PB_RuntimeShaderFlag:
        assert(ParamBind->m_pData);
        if (ParamBind->m_pData)
        {
          if (!r->m_RP.m_pShader)
            pData = NULL;
          else
          {
            bool bVal = (r->m_RP.m_FlagsShader_RT & ParamBind->m_pData->d.nData64[nComp]) != 0;
            sData[0].f[nComp] = (float)(bVal);
          }
        }
        break;
      case ECGP_PB_Scalar:
        assert(ParamBind->m_pData);
        if (ParamBind->m_pData)
          sData[0].f[nComp] = ParamBind->m_pData->d.fData[nComp];
        break;

      case ECGP_PB_CausticsParams:
        sData[0].f[0] = SCGParamsPF::pCausticsParams.x;
        sData[0].f[1] = SCGParamsPF::pCausticsParams.y;
        sData[0].f[2] = SCGParamsPF::pCausticsParams.z;
        sData[0].f[3] = 1.0f;
        break;

      case ECGP_PF_SunColor:
        {
          I3DEngine *p3DEngine = gEnv->p3DEngine;
          Vec3 pSunColor = p3DEngine->GetSunColor();
          sData[0].f[0] = pSunColor.x;
          sData[0].f[1] = pSunColor.y;
          sData[0].f[2] = pSunColor.z;
          sData[0].f[3] = 0.5f/r->GetWidth(); // pass screen half pixel size in alpha of sun and sky colors
          break;
        }
      case ECGP_PF_SkyColor:
        {
          I3DEngine *p3DEngine = gEnv->p3DEngine;
          Vec3 pSkyColor = p3DEngine->GetSkyColor();
          sData[0].f[0] = pSkyColor.x;
          sData[0].f[1] = pSkyColor.y;
          sData[0].f[2] = pSkyColor.z;
          sData[0].f[3] = 0.5f/r->GetHeight(); // pass screen half pixel size in alpha of sun and sky colors

          if( CRenderer::CV_r_postprocess_effects && CRenderer::CV_r_nightvision )
          {
            // If nightvision active, brighten up ambient
            using namespace NPostEffects;
            static CEffectParam *pParam = PostEffectMgr().GetByName("NightVision_Active"); 
            float fActive( pParam->GetParam() );
            if( fActive )
            {
              sData[0].f[0] += 0.25f;//0.75f;
              sData[0].f[1] += 0.25f;//0.75f;
              sData[0].f[2] += 0.25f;//0.75f;  
            }
          }

          break;
        }

      case ECGP_PB_CausticsSmoothSunDirection:
        {
          Vec3 pSun = Vec3(0.0f,0.0f,0.0f);
          I3DEngine *pEng = gEnv->p3DEngine;  
          static int nFrameID = gRenDev->GetFrameID(false);
          static Vec3 vCurrSunDir =  pEng->GetRealtimeSunDirNormalized();

          // Caustics are done with projection from sun - ence they update too fast with regular
          // sun direction. Use a smooth sun direction update instead to workaround this
          if( nFrameID != gRenDev->GetFrameID(false) )
          {
            nFrameID = gRenDev->GetFrameID(false);

            const float fSnapDot = 0.98f; 
            float fDot = fabs(vCurrSunDir.Dot(pEng->GetRealtimeSunDirNormalized()));
            if( fDot < fSnapDot )
              vCurrSunDir = pEng->GetRealtimeSunDirNormalized();   

            vCurrSunDir += (pEng->GetRealtimeSunDirNormalized() - vCurrSunDir) * 0.005f * gEnv->pTimer->GetFrameTime();
            vCurrSunDir.Normalize(); 
          }

          pSun = vCurrSunDir;

          sData[0].f[0] = pSun.x;
          sData[0].f[1] = pSun.y;
          sData[0].f[2] = pSun.z;
          sData[0].f[3] = 0;
          break;
        }

      case ECGP_PF_SunDirection:
        pEng = iSystem->GetI3DEngine();  
        v = pEng->GetSunDirNormalized();
        if((r->m_RP.m_PersFlags & RBPF_MAKESPRITE) && (SRendItem::m_RecurseLevel==2) && r->m_RP.m_DLights[2].Num())
        {
          v = r->m_RP.m_DLights[2][0]->m_Origin;
          v.Normalize();
        }
        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 0;
        break;
      case ECGP_PF_FogColor:
        sGetVolumetricFogParams(r);
        sData[0].f[3] = sData[0].f[2];
        sData[0].f[0] = r->m_FS.m_CurColor[0];
        sData[0].f[1] = r->m_FS.m_CurColor[1];
        sData[0].f[2] = r->m_FS.m_CurColor[2];
        //sData[0].f[3] = r->m_fAdaptedSceneScale;
        break;
      case ECGP_PF_CameraPos:
        v = r->GetRCamera().Orig;
        sData[0].f[0] = v.x;
        sData[0].f[1] = v.y;
        sData[0].f[2] = v.z;
        sData[0].f[3] = 1.f;
        break;
      case ECGP_PF_ScreenSize:
        sGetScreenSize(r);
        break;
      case ECGP_PF_Time:
        //sData[0].f[nComp] = r->m_RP.m_ShaderCurrTime; //r->m_RP.m_RealTime;
        sData[0].f[nComp] = r->m_RP.m_RealTime;
        assert(ParamBind->m_pData);
        if (ParamBind->m_pData)
          sData[0].f[nComp] *= ParamBind->m_pData->d.fData[nComp];
        break;
      case ECGP_PF_NearFarDist:
        {
          const CRenderCamera& rc = r->GetRCamera();
          pEng = iSystem->GetI3DEngine();
          sData[0].f[0] = rc.Near;
          sData[0].f[1] = rc.Far;
          // NOTE : v[2] is used to put the weapon's depth range into correct relation to the whole scene 
          // when generating the depth texture in the z pass (_RT_NEAREST) 
          sData[0].f[2] = rc.Far / pEng->GetMaxViewDistance(); 
          sData[0].f[3] = 1.0f / rc.Far;
        }
        break;
#ifndef EXCLUDE_SCALEFORM_SDK
      case ECGP_Matr_PB_SFCompMat:
        {
          const SSF_GlobalDrawParams* pParams(r->SF_GetGlobalDrawParams());
          assert(pParams);
          if (pParams)
          {
            int viewportX0, viewportY0, viewportWidth, viewportHeight;
            r->GetViewport(&viewportX0, &viewportY0, &viewportWidth, &viewportHeight);

            Matrix44& matComposite((Matrix44&)sData[0].f[0]);
            Matrix44 matProj;
            mathMatrixOrthoOffCenterLH(&matProj, (f32) viewportX0, (f32) (viewportX0 + viewportWidth), (f32) (viewportY0 + viewportHeight), (f32) viewportY0, 0.0f, 1.0f);
            matProj.Transpose();
#if defined(DIRECT3D9)
            matProj.m03 -= 1.0f / (float) viewportWidth;
            matProj.m13 += 1.0f / (float) viewportHeight;
#endif
            Matrix44 matTrans(*pParams->pTransMat);
            matComposite = matProj * matTrans;
          }
          break;
        }
      case ECGP_Matr_PB_SFTexGenMat0:
        {
          const SSF_GlobalDrawParams* pParams(r->SF_GetGlobalDrawParams());
          assert(pParams);
          if (pParams)
          {
            const Matrix34& mat(pParams->texture[0].texGenMat);
            sData[0].f[0] = mat.m00;
            sData[0].f[1] = mat.m01;
            sData[0].f[2] = mat.m02;
            sData[0].f[3] = mat.m03;

            sData[1].f[0] = mat.m10;
            sData[1].f[1] = mat.m11;
            sData[1].f[2] = mat.m12;
            sData[1].f[3] = mat.m13;
          }
          break;
        }
      case ECGP_Matr_PB_SFTexGenMat1:
        {
          const SSF_GlobalDrawParams* pParams(r->SF_GetGlobalDrawParams());
          assert(pParams);
          if (pParams)
          {
            const Matrix34& mat(pParams->texture[1].texGenMat);
            sData[0].f[0] = mat.m00;
            sData[0].f[1] = mat.m01;
            sData[0].f[2] = mat.m02;
            sData[0].f[3] = mat.m03;

            sData[1].f[0] = mat.m10;
            sData[1].f[1] = mat.m11;
            sData[1].f[2] = mat.m12;
            sData[1].f[3] = mat.m13;
          }
          break;
        }
      case ECGP_PB_SFBitmapColorTransform:
        {
          const SSF_GlobalDrawParams* pParams(r->SF_GetGlobalDrawParams());
          assert(pParams);
          if (pParams)
          {
            const ColorF& col1st(pParams->colTransform1st);
            sData[0].f[0] = col1st.r;
            sData[0].f[1] = col1st.g;
            sData[0].f[2] = col1st.b;
            sData[0].f[3] = col1st.a;

            const ColorF& col2nd(pParams->colTransform2nd);
            sData[1].f[0] = col2nd.r;
            sData[1].f[1] = col2nd.g;
            sData[1].f[2] = col2nd.b;
            sData[1].f[3] = col2nd.a;
          }
          break;
        }
#endif
      case ECGP_Matr_PI_OceanMat:
        {
          const CRenderCamera& cam(r->GetRCamera());

          Matrix44 viewMat;				
          viewMat.m00 = cam.X.x; viewMat.m01 = cam.Y.x; viewMat.m02 = cam.Z.x; viewMat.m03 = 0;					
          viewMat.m10 = cam.X.y; viewMat.m11 = cam.Y.y; viewMat.m12 = cam.Z.y; viewMat.m13 = 0;
          viewMat.m20 = cam.X.z; viewMat.m21 = cam.Y.z; viewMat.m22 = cam.Z.z; viewMat.m23 = 0;
          viewMat.m30 = 0; viewMat.m31 = 0; viewMat.m32 = 0; viewMat.m33 = 1;

          mathMatrixMultiply(&sData[0].f[0], (float*)r->m_matProj->GetTop(), (float*)&viewMat.m00, g_CpuFlags);					
          mathMatrixTranspose(&sData[0].f[0], &sData[0].f[0], g_CpuFlags);
          break;
        }
      case ECGP_PB_CloudShadingColorSun:
        sData[0].f[0] = SCGParamsPF::pCloudShadingColorSun.x;
        sData[0].f[1] = SCGParamsPF::pCloudShadingColorSun.y;
        sData[0].f[2] = SCGParamsPF::pCloudShadingColorSun.z;
        sData[0].f[3] = 0;
        break;

      case ECGP_PB_CloudShadingColorSky:
        sData[0].f[0] = SCGParamsPF::pCloudShadingColorSky.x;
        sData[0].f[1] = SCGParamsPF::pCloudShadingColorSky.y;
        sData[0].f[2] = SCGParamsPF::pCloudShadingColorSky.z;
        sData[0].f[3] = 0;
        break;

      case ECGP_PB_ResInfoDiffuse:
        {
          SRenderShaderResources* pRes(gcpRendD3D->m_RP.m_pShaderResources);
          assert(pRes && pRes->m_Textures[EFTT_DIFFUSE] && pRes->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex);
          ITexture* pTexture(pRes->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex);
          assert(pTexture);
          int texWidth(pTexture->GetWidth());
          int texHeight(pTexture->GetHeight());						
          sData[0].f[0] = (float) texWidth;
          sData[0].f[1] = (float) texHeight;
          sData[0].f[2] = 1.0f / (float) texWidth;
          sData[0].f[3] = 1.0f / (float) texHeight;
          break;
        }
      case ECGP_PB_ResInfoBump:
        {
//PS3HACK
#if !defined(PS3)
          SRenderShaderResources* pRes(gcpRendD3D->m_RP.m_pShaderResources);
          assert(pRes && pRes->m_Textures[EFTT_BUMP] && pRes->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex);
          ITexture* pTexture(pRes->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex);
          assert(pTexture);
          int texWidth(pTexture->GetWidth());
          int texHeight(pTexture->GetHeight());						
          sData[0].f[0] = (float) texWidth;
          sData[0].f[1] = (float) texHeight;
          sData[0].f[2] = 1.0f / (float) texWidth;
          sData[0].f[3] = 1.0f / (float) texHeight;
#endif
          break;
        }
			case ECGP_PB_TexAtlasSize:
				{
					float size = r->CV_r_texatlassize;
					float sizeInv = 1.0f / size;
					sData[0].f[0] = size;
					sData[0].f[1] = size;
					sData[0].f[2] = sizeInv;
					sData[0].f[3] = sizeInv;
					break;
				}

      default:
        break;
        //Warning("Unknown Parameter '%s' of type %d", ParamBind->m_Name.c_str(), ParamBind->m_eCGParamType);
        //assert(0);
        //return NULL;
      }
      if (ParamBind->m_Flags & PF_SINGLE_COMP)
        break;
    }
    if (pSrc)
    {
      if (pDst)
      {
#if defined(PS3) && defined(PS3_OPT)
				const float *const __restrict cpSrc = pSrc;
				float *const __restrict cpDst = pDst;
				const uint32 cParamCnt= nParams;
				for(uint32 i=0; i<cParamCnt; i+=4)
				{
					cpDst[i]	 = cpSrc[i];
					cpDst[i+1] = cpSrc[i+1];
					cpDst[i+2] = cpSrc[i+2];
					cpDst[i+3] = cpSrc[i+3];
				}
#else
        memcpy(pDst, pSrc, nParams*4*sizeof(float));
#endif
        pDst += nParams*4;
      }
      else
      {
        // in WIN32 pData must be 16 bytes aligned
        assert(!((uint)pSrc & 0xf) || sizeof(void *)!=4);
#if !defined (DIRECT3D10)
        if (!(ParamBind->m_Flags & PF_INTEGER))
          mfParameterfA(ParamBind, pSrc, nParams, eSH);
        else
          mfParameteri(ParamBind, pSrc, eSH);
#else
        if (!(ParamBind->m_Flags & PF_INTEGER))
          mfParameterfA(ParamBind, pSrc, nParams, eSH, nMaxVecs);
        else
          mfParameteri(ParamBind, pSrc, eSH, nMaxVecs);
#endif
      }
    }
  }
  return pDst;
}

void CHWShader_D3D::mfSetParameters(int nType)
{
  SHWSInstance *pInst = m_pCurInst;
  if (pInst->m_pParams[nType])
  {
#if defined (DIRECT3D10)
    mfSetParameters(&(*pInst->m_pParams[nType])[0], pInst->m_pParams[nType]->size(), NULL, m_eSHClass, pInst->m_nMaxVecs[nType]);
#else
    mfSetParameters(&(*pInst->m_pParams[nType])[0], pInst->m_pParams[nType]->size(), NULL, m_eSHClass);
#endif
  }
}


void CHWShader_D3D::mfPrecache()
{
  bool bPrevFog = gRenDev->m_FS.m_bEnable;
  gRenDev->m_FS.m_bEnable = true;

  uint64 RTMask = 0;
  uint LTMask = 0;
  uint GLMask = m_nMaskGenShader;

  mfGetInstance(RTMask, LTMask, GLMask, 0, 0, HWSF_PRECACHE_INST);
  mfActivate(0);

  gRenDev->m_FS.m_bEnable = bPrevFog;  
}

//=========================================================================================

void CHWShader_D3D::mfReset(uint32 CRC32)
{
  uint i;
  for (i=0; i<m_Insts.Num(); i++)
  {
    m_pCurInst = &m_Insts[i];
    m_pCurInst->Release(m_pCache);
  }
  m_pCurInst = NULL;
  m_Insts.Free();
  if (CRC32!=0)
  {
    // Delete all shared instances for this shader if time is different
    InstanceMapItor itInst = m_SharedInsts.find(m_EntryFunc);
    SHWSSharedList *pInstSH = NULL;
    if (itInst != m_SharedInsts.end())
    {
      pInstSH = itInst->second;
      int i, j;
      const char *nm = gRenDev->m_RP.m_pShader->m_NameShader.c_str();
      SHWSSharedName *pSHN;
      for (i=0; i<pInstSH->m_SharedNames.size(); i++)
      {
        pSHN = &pInstSH->m_SharedNames[i];
        if (!stricmp(pSHN->m_Name.c_str(), nm))
          break;
      }
      if (i != pInstSH->m_SharedNames.size())
      {
        if (pSHN->m_CRC32 != CRC32)
        {
          pSHN->m_CRC32 = CRC32;
          for (i=0; i<pInstSH->m_SharedInsts.Num(); i++)
          {
            SHWSSharedInstance *pSHI = &pInstSH->m_SharedInsts[i];
            for (j=0; j<pSHI->m_Insts.Num(); j++)
            {
              m_pCurInst = &pSHI->m_Insts[j];
              m_pCurInst->Release(m_pCache);
            }
            pSHI->m_Insts.Free();
          }
          pInstSH->m_SharedInsts.Free();
        }
      }
      else
      {
        int nnn = 0;
      }
    }
  }

  mfCloseCacheFile();
}

CHWShader_D3D::~CHWShader_D3D()
{
  mfFree(0);
}

static TArray<char> sNewScr;

static bool sGetMask(char *str, SShaderGen *pGen, uint64& nMask)
{
  int i;

  for (i=0; i<pGen->m_BitMask.Num(); i++)
  {
    SShaderGenBit *pBit = pGen->m_BitMask[i];
    if (!strcmp(str, pBit->m_ParamName.c_str()))
    {
      nMask |= pBit->m_Mask;
      return true;
    }
  }
  return false;
}

char *CHWShader_D3D::mfGenerateScript(SHWSInstance *&pInst, DynArray<SCGBind>& InstBindVars, uint nFlags)
{
  uint i;
  char *cgs = NULL;

  sNewScr.SetUse(0);
  assert (!m_TokenData.empty());

  DynArray<uint32> NewTokens;

  uint32 eT = eT_unknown;

  switch (pInst->m_eProfileType)
  {
  case eHWSP_VS_1_1:
  case eHWSP_VS_2_0:
  case eHWSP_VS_3_0:
  case eHWSP_VS_4_0:
    eT = eT__VS;
    break;

  case eHWSP_PS_1_1:
  case eHWSP_PS_1_4:
  case eHWSP_PS_2_0:
  case eHWSP_PS_2_X:
  case eHWSP_PS_3_0:
  case eHWSP_PS_4_0:
    eT = eT__PS;
    break;

  case eHWSP_GS_4_0:
    eT = eT__GS;
    break;

  default:
    assert(0);
  }
  if (eT != eT_unknown)
    CParserBin::AddDefineToken(eT, NewTokens);

  // Include runtime mask definitions in the script
  SShaderGen *shg = gRenDev->m_cEF.m_pGlobalExt;
  if (shg && pInst->m_RTMask)
  {
    for (i=0; i<shg->m_BitMask.Num(); i++)
    {
      SShaderGenBit *bit = shg->m_BitMask[i];
      if (!(bit->m_Mask & pInst->m_RTMask))
        continue;
      CParserBin::AddDefineToken(bit->m_dwToken, NewTokens);
    } 
  }

  // Include light mask definitions in the script
  if (m_Flags & HWSG_SUPPORTS_MULTILIGHTS)
  {
    int nLights = pInst->m_LightMask & 0xf;
    if (nLights)
      CParserBin::AddDefineToken(eT__LT_LIGHTS, NewTokens);
    CParserBin::AddDefineToken(eT__LT_NUM, nLights+eT_0, NewTokens);
    bool bHasProj = false;
    for (int i=0; i<4; i++)
    {
      int nLightType = (pInst->m_LightMask >> (SLMF_LTYPE_SHIFT + i*SLMF_LTYPE_BITS)) & SLMF_TYPE_MASK;
      if (nLightType == SLMF_PROJECTED)
        bHasProj = true;

      CParserBin::AddDefineToken(eT__LT_0_TYPE+i, nLightType+eT_0, NewTokens);
    }
    if (bHasProj)
      CParserBin::AddDefineToken(eT__LT_HASPROJ, eT_1, NewTokens);
  }
  else
  if (m_Flags & HWSG_SUPPORTS_LIGHTING)
  {
    CParserBin::AddDefineToken(eT__LT_LIGHTS, NewTokens);
    int nLightType = (pInst->m_LightMask >> SLMF_LTYPE_SHIFT) & SLMF_TYPE_MASK;
    if (nLightType == SLMF_PROJECTED)
      CParserBin::AddDefineToken(eT__LT_HASPROJ, eT_1, NewTokens);
  }

  // Include modificator mask definitions in the script
  if ((m_Flags & HWSG_SUPPORTS_MODIF) && pInst->m_MDMask)
  {
    for (int nt=0; nt<4; nt++)
    {
      uint tcGOLMask = HWMD_TCGOL0<<nt;
      uint tcGRMMask = HWMD_TCGRM0<<nt;
      uint tcGNMMask = HWMD_TCGNM0<<nt;
      uint tcGSMMask = HWMD_TCGSM0<<nt;
      uint tcMMask   = HWMD_TCM0<<nt;
      uint tcProjMask = HWMD_TCPROJ0<<nt;
      uint tcTypeMask = HWMD_TCTYPE0<<nt;
      if (pInst->m_MDMask & tcTypeMask)
        CParserBin::AddDefineToken(eT__TT0_TCUBE+nt, NewTokens);
      if (pInst->m_MDMask & tcProjMask)
        CParserBin::AddDefineToken(eT__TT0_TCPROJ+nt, NewTokens);
      if (pInst->m_MDMask & tcMMask)
        CParserBin::AddDefineToken(eT__TT0_TCM+nt, NewTokens);
      if (pInst->m_MDMask & (tcGOLMask | tcGRMMask | tcGNMMask | tcGSMMask))
      {
        int nType = 0;
        if (pInst->m_MDMask & tcGOLMask)
          nType = 1;
        else
        if (pInst->m_MDMask & tcGRMMask)
          nType = 2;
        else
        if (pInst->m_MDMask & tcGNMMask)
          nType = 3;
        else
        if (pInst->m_MDMask & tcGSMMask)
          nType = 4;
        CParserBin::AddDefineToken(eT__TT0_TCG_TYPE, eT_0+nType, NewTokens);
      }
    }
  }

  // Include vertex modificator mask definitions in the script
  if ((m_Flags & HWSG_SUPPORTS_VMODIF) && pInst->m_MDVMask)
  {
    int nType = pInst->m_MDVMask & 0xf;
    if (nType)
      CParserBin::AddDefineToken(eT__VT_TYPE, eT_0+nType, NewTokens);
    if ((pInst->m_MDVMask & MDV_BENDING) || nType == eDT_Bending)
    {
      CParserBin::AddDefineToken(eT__VT_BEND, eT_1, NewTokens);
      if (!(pInst->m_MDVMask & 0xf))
      {
        nType = eDT_Bending;
        CParserBin::AddDefineToken(eT__VT_TYPE, eT_0+nType, NewTokens);
      }
    }
    if (pInst->m_MDVMask & MDV_DEPTH_OFFSET)
      CParserBin::AddDefineToken(eT__VT_DEPTH_OFFSET, eT_1, NewTokens);
    if (pInst->m_MDVMask & MDV_WIND)
      CParserBin::AddDefineToken(eT__VT_WIND, eT_1, NewTokens);
    if (pInst->m_MDVMask & MDV_DET_BENDING)
      CParserBin::AddDefineToken(eT__VT_DET_BEND, eT_1, NewTokens);
    if (pInst->m_MDVMask & MDV_DET_BENDING_GRASS)
      CParserBin::AddDefineToken(eT__VT_GRASS, eT_1, NewTokens);
    if (pInst->m_MDVMask & MDV_TERRAIN_ADAPT)
      CParserBin::AddDefineToken(eT__VT_TERRAIN_ADAPT, eT_1, NewTokens);
    if (pInst->m_MDVMask & ~0xf)
      CParserBin::AddDefineToken(eT__VT_TYPE_MODIF, eT_1, NewTokens);
  }

  if (m_Flags & HWSG_FP_EMULATION)
  {
    CParserBin::AddDefineToken(eT__FT0_COP, eT_0+(pInst->m_LightMask&0xff), NewTokens);
    CParserBin::AddDefineToken(eT__FT0_AOP, eT_0+((pInst->m_LightMask&0xff00)>>8), NewTokens);

    byte CO_0 = ((pInst->m_LightMask&0xff0000) >> 16) & 7;
    CParserBin::AddDefineToken(eT__FT0_CARG1, eT_0+CO_0, NewTokens);

    byte CO_1 = ((pInst->m_LightMask&0xff0000) >> 19) & 7;
    CParserBin::AddDefineToken(eT__FT0_CARG2, eT_0+CO_1, NewTokens);

    byte AO_0 = ((pInst->m_LightMask&0xff000000) >> 24) & 7;
    CParserBin::AddDefineToken(eT__FT0_AARG1, eT_0+AO_0, NewTokens);

    byte AO_1 = ((pInst->m_LightMask&0xff000000) >> 27) & 7;
    CParserBin::AddDefineToken(eT__FT0_AARG2, eT_0+AO_1, NewTokens);

    if (CO_0 == eCA_Specular || CO_1 == eCA_Specular || AO_0 == eCA_Specular || AO_1 == eCA_Specular)
      CParserBin::AddDefineToken(eT__FT_SPECULAR, NewTokens);
    if (CO_0 == eCA_Diffuse || CO_1 == eCA_Diffuse || AO_0 == eCA_Diffuse || AO_1 == eCA_Diffuse)
      CParserBin::AddDefineToken(eT__FT_DIFFUSE, NewTokens);
    if (CO_0 == eCA_Texture || CO_1 == eCA_Texture || AO_0 == eCA_Texture || AO_1 == eCA_Texture)
      CParserBin::AddDefineToken(eT__FT_TEXTURE, NewTokens);
  }

  int nT = NewTokens.size();
  NewTokens.resize(nT + m_TokenData.size());
  memcpy(&NewTokens[nT], &m_TokenData[0], m_TokenData.size()*sizeof(uint32));

  CParserBin Parser(NULL, gRenDev->m_RP.m_pShader);
  Parser.Preprocess(1, NewTokens, &m_Table);
  CorrectScriptEnums(Parser, pInst, InstBindVars);
  RemoveUnaffectedParameters_D3D10(Parser, pInst, InstBindVars);
  CParserBin::ConvertToAscii(&Parser.m_Tokens[0], Parser.m_Tokens.size(), m_Table, sNewScr);

 /* FILE *fp = iSystem->GetIPak()->FOpen("fff", "w");
  if (fp)
  {
    iSystem->GetIPak()->FPrintf(fp, "%s", &sNewScr[0]);
    iSystem->GetIPak()->FClose (fp);
  }*/

  return &sNewScr[0];
}

/*static uint32 sFindVar(CParserBin& Parser, int& nStart)
{
  const uint32 *pTokens = Parser.GetTokens(0);
  int nLast = Parser.GetNumTokens()-1;

  while (nStart <= nLast)
  {
    if (pTokens[nStart] == eT_br_cv_1)
    {
      int nRecurs = 1;
      nStart++;
      while(nStart <= nLast)
      {
        if (pTokens[nStart++] == eT_br_cv_1)
          nRecurs++;
        else
        if (pTokens[nStart++] == eT_br_cv_2)
        {
          nRecurs--;
          if (nRecurs == 0)
            break;
        }
      }
    }
    if (nStart <= nLast)
      break;
    if (pTokens[nStart] >= eT_float && pTokens[nStart] <= eT_int)
    {
      if (nStart+3 <= nLast) 
      {
        uint32 nName = pTokens[nStart+1];
        uint32 nN = pTokens[nStart+2];
        if (nN != eT_colon)
        {
          if (nN == eT_br_sq_1)
          {
            assert(pTokens[nStart+4] == eT_br_sq_2);
            if (pTokens[nStart+4] == eT_br_sq_2)
              nN = pTokens[nStart+5];
          }
        }
        if (nN == eT_colon)
          return nName;
        nStart += 3;
      }
      else
        break;
    }
    nStart++;
  }
  nStart = -1;
  return 0;
}

bool sIsAffectFuncs(CParserBin& Parser, uint32 nName)
{
  const uint32 *pTokens = Parser.GetTokens(0);
  int nStart = 0;
  int nLast = Parser.GetNumTokens()-1;

  while (nStart <= nLast)
  {
    if (pTokens[nStart] == eT_br_cv_1)
    {
      int nRecurs = 1;
      nStart++;
      int nBegin = nStart;
      while(nStart <= nLast)
      {
        if (pTokens[nStart++] == eT_br_cv_1)
          nRecurs++;
        else
        if (pTokens[nStart++] == eT_br_cv_2)
        {
          nRecurs--;
          if (nRecurs == 0)
            break;
        }
      }
      if (nStart <= nLast)
        break;
      int nPos = Parser.FindToken(nBegin, nStart, nName);
      if (nPos >= 0)
        return true;
    }
    nStart++;
  }
  return false;
}*/

void CHWShader_D3D::RemoveUnaffectedParameters_D3D10(CParserBin& Parser, SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars)
{
#ifdef DIRECT3D10
  int nPos = Parser.FindToken(0, Parser.m_Tokens.size()-1, eT_cbuffer);
  while (nPos >= 0)
  {
    uint32 nName = Parser.m_Tokens[nPos+1];
    if (nName == eT_PER_BATCH || nName == eT_PER_INSTANCE)
    {
      int nPosEnd = Parser.FindToken(nPos+3, Parser.m_Tokens.size()-1, eT_br_cv_2);
      assert(nPosEnd >= 0);
      int nPosN = Parser.FindToken(nPos+1, Parser.m_Tokens.size()-1, eT_br_cv_1);
      assert(nPosN >= 0);
      nPosN++;
      while (nPosN < nPosEnd)
      {
        uint32 nT = Parser.m_Tokens[nPosN+1];
        int nPosCode = Parser.FindToken(nPosEnd+1, Parser.m_Tokens.size()-1, nT);
        if (nPosCode < 0)
        {
          assert(nPosN > 0 && nPosN < Parser.m_Tokens.size());
          int i;
          if (InstBindVars.size())
          {
            CCryName nm = Parser.GetString(nT);
            for (i=0; i<InstBindVars.size(); i++)
            {
              SCGBind &b = InstBindVars[i];
              if (b.m_Name == nm)
                break;
            }
            if (i == InstBindVars.size())
              Parser.m_Tokens[nPosN] = eT_comment;
          }
          else
            Parser.m_Tokens[nPosN] = eT_comment;
        }
        nPosN = Parser.FindToken(nPosN+2, nPosEnd, eT_semicolumn);
        assert(nPosN >= 0);
        nPosN++;
      }
      nPos = Parser.FindToken(nPosEnd+1, Parser.m_Tokens.size()-1, eT_cbuffer);
    }
    else
      nPos = Parser.FindToken(nPos+2, Parser.m_Tokens.size()-1, eT_cbuffer);
  }
#else
  /*int nStart = 0;
  while (true)
  {
    uint32 nName = sFindVar(Parser, nStart);
    if (nStart < 0)
      break;
    bool bAffect = sIsAffectFuncs(Parser, nName);
    if (!bAffect)
      Parser.m_Tokens[nStart] = eT_comment;
    nStart++;
  }*/
#endif
}

void CHWShader_D3D::CorrectScriptEnums(CParserBin& Parser, SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars)
{
  // correct enumeration of TEXCOORD# interpolators after preprocessing
  int nCur = 0;
  int nSize = Parser.m_Tokens.size();
  uint32 *pTokens = &Parser.m_Tokens[0];
  int nInstParam = 0;
  const uint32 Toks[] = {eT_TEXCOORDN, eT_TEXCOORDN_centroid, eT_unknown};
  while (true)
  {
    nCur = Parser.FindToken(nCur, nSize-1, eT_struct);
    if (nCur < 0)
      break;
    int nLastStr = Parser.FindToken(nCur, nSize-1, eT_br_cv_2);
    assert(nLastStr >= 0);
    if (nLastStr < 0)
      break;
    int n = 0;
    while (nCur < nLastStr)
    {
      int nTN = Parser.FindToken(nCur, nLastStr, Toks);
      if (nTN < 0)
      {
        nCur = nLastStr+1;
        break;
      }
      assert(pTokens[nTN-1] == eT_colon);
      int nArrSize = 1;
      uint32 nTokName;
      if (pTokens[nTN-2] == eT_br_sq_2)
      {
        nArrSize = pTokens[nTN-3] - eT_0;
        assert(pTokens[nTN-4] == eT_br_sq_1);
        nTokName = pTokens[nTN-5];
      }
      else
      {
        uint32 nType = pTokens[nTN-3];
        assert(nType==eT_float || nType==eT_float2 || nType==eT_float3 || nType==eT_float4 || nType==eT_float4x4 || nType==eT_float3x4 || nType==eT_float2x4 || nType==eT_float3x3);
        if (nType == eT_float4x4)
          nArrSize = 4;
        else
        if (nType == eT_float3x4 || nType == eT_float3x3)
          nArrSize = 3;
        else
        if (nType == eT_float2x4)
          nArrSize = 2;
        nTokName = pTokens[nTN-2];
      }
      assert(nArrSize>0 && nArrSize<16);
//PS3 not a hack, PS3 has no centroid filtering!
#if defined(PS3)
      EToken eT = eT_TEXCOORD0;
#else
      EToken eT = (pTokens[nTN]==eT_TEXCOORDN) ? eT_TEXCOORD0 : eT_TEXCOORD0_centroid;
#endif
      n = min(n, 15); 

      pTokens[nTN] = n+eT;
      n += nArrSize;
      nCur = nTN+1;
      if (pInst->m_RTMask & g_HWSR_MaskBit[HWSR_INSTANCING_ATTR])
      {
        const char *szName = Parser.GetString(nTokName, m_Table);
        if (!strnicmp(szName, "Inst", 4))
        {
          char newName[256];
          int nm = 0;
          while(szName[4+nm] > 0x20 && szName[4+nm] != '[')
          {
            newName[nm] = szName[4+nm];
            nm++;
          }
          newName[nm++] = 0;

          SCGBind bn;
          bn.m_dwBind = nInstParam;
          bn.m_nParameters = nArrSize;
          bn.m_Name = newName;
          InstBindVars.push_back(bn);

          nInstParam += nArrSize;
        }
      }
    }
  }
  pInst->m_nNumInstAttributes = nInstParam;
}


bool CHWShader_D3D::mfSetSamplers()
{
  //PROFILE_FRAME(Shader_SetShaderSamplers);
  SHWSInstance *pInst = m_pCurInst;
  if (!pInst)
    return false;
  if (!pInst->m_pSamplers)
    return true;
  uint i;
  CD3D9Renderer *rd = gcpRendD3D;
  SRenderShaderResources *pSR = rd->m_RP.m_pShaderResources;

  const int nSize = pInst->m_pSamplers->size();
  STexSampler *pSamp = &(*pInst->m_pSamplers)[0];
  for (i=0; i<nSize; i++, pSamp++)
  {
    CTexture *tx = pSamp->m_pTex;
    assert(tx);
    if (!tx)
      continue;
    int nSetID = -1;
    int nTexSlot = -1;
    int nSamplerSlot = pSamp->m_nSamplerSlot;
    assert(nSamplerSlot >= 0);
    STexSampler *pSM = pSamp;
    int nTS = pSM->m_nTexState;
#if defined (DIRECT3D9) || defined(OPENGL)
    CTexture::m_CurStage = nSamplerSlot;
#else
    CTexture::m_CurStage = i;
#endif
    if (tx >= &CTexture::m_Templates[0] && tx <= &CTexture::m_Templates[EFTT_MAX-1])
    {
      if (tx >= &CTexture::m_Templates[EFTT_LIGHTMAP] && tx <= &CTexture::m_Templates[EFTT_OCCLUSION])
      {
        if (tx == &CTexture::m_Templates[EFTT_RAE] )
          nSetID = rd->m_RP.m_pCurObject->m_nRAEId ? rd->m_RP.m_pCurObject->m_nRAEId : CTexture::m_Text_White->GetID();
      }
      else
      {
        nTexSlot = tx - &CTexture::m_Templates[0];

        if (!pSR || !pSR->m_Textures[nTexSlot])
        {
          tx = CTexture::m_Text_NoTexture;           

          if (nTexSlot)
          {
            // if no texture, pass dummy textures (to minimize shader permutations)
            tx = CTexture::m_Text_White;

            if(nTexSlot == EFTT_DECAL_OVERLAY )
            {
              tx = CTexture::m_Text_Gray;
            }
            else
            if (nTexSlot == EFTT_BUMP)
            {
              tx = CTexture::m_Text_FlatBump;
            }
            else
            if (nTexSlot == EFTT_BUMP_DIFFUSE)
            {
              if (pSR && pSR->m_Textures[EFTT_BUMP]) 
              {
                pSM = &pSR->m_Textures[EFTT_BUMP]->m_Sampler;
                tx = pSM->m_pTex;
              }
              else
                tx = CTexture::m_Text_FlatBump;
            }
          }
        }
        else
        {
          pSM = &pSR->m_Textures[nTexSlot]->m_Sampler;
          tx = pSM->m_pTex;
          if (nTS<0 || !CTexture::m_TexStates[nTS].m_bActive)
            nTS = pSM->m_nTexState;   // Use material texture state
          if (pSM->m_pDynTexSource)
          {
            if (pSM->m_pDynTexSource->Apply(-1, nTS))
              continue;
            else
              tx = CTexture::m_Text_White;
          }
        }
      }
    }
    if (pSM && pSM->m_pAnimInfo)
      pSM->Update();
    //    assert(tx);
    if (!tx)
      continue;

    if (nSetID > 0)
    {
      CTexture::ApplyForID(nSetID, nTS, nSamplerSlot);
    }
    else
    {
      int nCustomID = tx->GetCustomID();
      if (nCustomID <= 0)
      {
				if (tx->UseDecalBorderCol())
				{
					STexState TS = CTexture::m_TexStates[nTS];
					//TS.SetFilterMode(...); // already set up
					TS.SetClampMode(TADDR_BORDER, TADDR_BORDER, TADDR_BORDER);
					TS.SetBorderColor(ColorF(1,1,1,0).pack_argb8888());
					nTS = CTexture::GetTexState(TS);
				}

        tx->Apply(-1, nTS, nTexSlot, nSamplerSlot);
      }
      else
      switch (nCustomID)
      {
        case TO_FROMRE0:
        case TO_FROMRE1:
          {
            if (rd->m_RP.m_pRE)
              nCustomID = rd->m_RP.m_pRE->m_CustomTexBind[nCustomID-TO_FROMRE0];
            else
              nCustomID = rd->m_RP.m_RECustomTexBind[nCustomID-TO_FROMRE0];
            if (nCustomID < 0)
              break;

            CTexture *pTex = CTexture::GetByID(nCustomID);
            pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot);

            //CTexture::ApplyForID(nCustomID, bSRGB);
          }
          break;

        case TO_ZTARGET_MS:
          {
            CTexture *pTex = CTexture::m_Text_ZTarget;
            assert(pTex);
            if (pTex)
              pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot, 4);
          }
          break;

        case TO_SHADOWID0:
        case TO_SHADOWID1:
        case TO_SHADOWID2:
        case TO_SHADOWID3:
        case TO_SHADOWID4:
        case TO_SHADOWID5:
        case TO_SHADOWID6:
        case TO_SHADOWID7:
          {
#if defined (DIRECT3D10)
            //TF reset custom res view after shadow pass
            int nCustomResViewID = rd->m_RP.m_ShadowCustomResViewID[nCustomID-TO_SHADOWID0];
#endif
            nCustomID = rd->m_RP.m_ShadowCustomTexBind[nCustomID-TO_SHADOWID0];

            if (nCustomID < 0)
              break;
            //force  MinFilter = Linear; MagFilter = Linear; for HW_PCF_FILTERING
            STexState TS = CTexture::m_TexStates[nTS];
            if (gRenDev->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ])
            {
              TS.SetFilterMode(FILTER_LINEAR);
#if defined (DIRECT3D10)
              if (nCustomResViewID>=0)
              {
                //texture array case
                TS.SetComparisonFilter(false);
              }
              else
              {
                //non texture array case
                TS.SetComparisonFilter(true);
              }

#endif
            }
            else
            {
              if (gRenDev->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[ HWSR_SHADOW_FILTER ])
              {
                TS.SetFilterMode(FILTER_LINEAR);
              }
              else
              {
                TS.SetFilterMode(FILTER_POINT);
              }
            }


            TS.PostCreate();

            CTexture* tx  = CTexture::GetByID(nCustomID);
#if defined (DIRECT3D10)
            tx->Apply(-1, CTexture::GetTexState(TS), nTexSlot, nSamplerSlot, nCustomResViewID);
#else
            tx->Apply(-1, CTexture::GetTexState(TS), nTexSlot, nSamplerSlot);
#endif

          }
          break;

        case TO_FROMRE0_FROM_CONTAINER:
        case TO_FROMRE1_FROM_CONTAINER:
          {
            // take render element from vertex container render mesh if available
            CRendElement *pRE = sGetContainerRE0(rd->m_RP.m_pRE);
            if (pRE)
              nCustomID = pRE->m_CustomTexBind[nCustomID-TO_FROMRE0_FROM_CONTAINER];
            else
              nCustomID = rd->m_RP.m_RECustomTexBind[nCustomID-TO_FROMRE0_FROM_CONTAINER];
            if (nCustomID < 0)
              break;
            CTexture::ApplyForID(nCustomID, nTS, nSamplerSlot);
          }
          break;

        case TO_LIGHTINFO:
          {
            CTexture *pTex = CTexture::m_Text_LightInfo[SRendItem::m_RecurseLevel-1];
            assert(pTex);
            if (pTex)
              pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          }
          break;

        case TO_SCREENSHADOWMAP:
          {
            CTexture *tx = NULL;
            int nCurLightGroup = rd->m_RP.m_nCurLightGroup;
            if( rd->m_RP.m_PersFlags2 & RBPF2_RAINPASS ) // special case for rain pass - make sure light group is 0
              nCurLightGroup = 0;

            if (nCurLightGroup>= 0)
            {
              int nGroup = nCurLightGroup>=0 ? nCurLightGroup : MAX_REND_LIGHT_GROUPS;
              if (SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup])
              {
                assert(nCurLightGroup>=0 && nCurLightGroup<MAX_REND_LIGHTS/4);
                //tx = CTexture::m_Text_ScreenShadowMap[rd->m_RP.m_nCurLightGroup];
                tx  = CTexture::m_Tex_CurrentScreenShadowMap[nCurLightGroup];
              }
              else
              {
                tx = CTexture::m_Text_Black;
              }
            }
            else
            {
              tx = CTexture::m_Text_Black;
            }
            assert(tx);
            if (tx)
              tx->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          }
          break;

				case TO_SCREEN_AO_TARGET:
					{
						CTexture *tx = NULL;
						if ((rd->m_RP.m_PersFlags & RBPF_ALLOW_AO) && rd->m_RP.m_nAOMaskUpdateLastFrameId == rd->m_nFrameUpdateID)
							tx = CTexture::m_Text_AOTarget;
						else
							tx = CTexture::m_Text_White;
						assert(tx);
						if (tx)
							tx->Apply(-1, nTS, nTexSlot, nSamplerSlot);
					}
					break;

        case TO_DOWNSCALED_ZTARGET_FOR_AO:
          {
            CTexture *tx = NULL;
            if (rd->CV_r_SSAO_downscale_ztarget)
              tx = CTexture::m_Text_ZTargetScaled;
            else
              tx = CTexture::m_Text_ZTarget;
            assert(tx);
            if (tx)
              tx->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          }
          break;

        case TO_FROMOBJ0:
          {
            if (rd->m_RP.m_pCurObject)
              nCustomID = rd->m_RP.m_pCurObject->m_nTextureID;
            if (nCustomID <= 0)
              return 0;

            //CTexture::ApplyForID(nCustomID, bSRGB, nTS);
            CTexture *pTex = CTexture::GetByID(nCustomID);
            pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot);

          }
          break;

				case TO_FROMOBJ1:
					{
						if (rd->m_RP.m_pCurObject)
							nCustomID = rd->m_RP.m_pCurObject->m_nTextureID1;
						if (nCustomID <= 0)
							return 0;
						//CTexture::ApplyForID(nCustomID, bSRGB, nTS);
            CTexture *pTex = CTexture::GetByID(nCustomID);
            pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot);
					}
					break;

        case TO_FROMLIGHT:
          {
            bool bRes = CTexture::SetProjector(-1, nTS, nSamplerSlot);
            if (!bRes && !(rd->m_RP.m_PersFlags & RBPF_MULTILIGHTS))
              Warning( "Couldn't set projected texture for light source (Shader: '%s')\n", rd->m_RP.m_pShader->GetName());
          }
          break;

        case TO_RT_CM:
          {
            SHRenderTarget *pRT = pSM->m_pTarget ? pSM->m_pTarget : pSamp->m_pTarget;
            assert(pRT);
            if (!pRT)
              break;
            SEnvTexture *pEnvTex = pRT->GetEnvCM();
            assert(pEnvTex->m_pTex);
            if (pEnvTex && pEnvTex->m_pTex)
              pEnvTex->m_pTex->Apply(-1, nTS);
          }
          break;

        case TO_RT_LCM:
          {
            Vec3 vPos = rd->m_RP.m_pCurObject->GetTranslation();
            SEnvTexture *pEnvTex = CTexture::FindSuitableEnvLCMap(vPos, true, 0, 0);
            assert(pEnvTex && pEnvTex->m_pTex);
            if (pEnvTex && pEnvTex->m_pTex)
              pEnvTex->m_pTex->Apply(-1, nTS);
          }
          break;

        case TO_RT_2D:
          {
            SHRenderTarget *pRT = pSM->m_pTarget ? pSM->m_pTarget : pSamp->m_pTarget;
            SEnvTexture *pEnvTex = pRT->GetEnv2D();
            //assert(pEnvTex->m_pTex);
            if (pEnvTex && pEnvTex->m_pTex)
              pEnvTex->m_pTex->Apply(-1, nTS);
          }
          break;

        case TO_SCREENMAP:
          //if (rd->m_RP.m_PersFlags & RBPF_HDR)
          //  CTexture::m_Text_ScreenMap_HDR->Apply(-1, nTS);
          //else
            CTexture::m_Text_ScreenMap->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          break;

        case TO_WATEROCEANMAP:
            CTexture::m_Text_WaterOcean->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          break;

        case TO_WATERVOLUMEMAP:
          {
            if( CTexture::m_Text_WaterVolumeDDN )
            {
              using namespace NPostEffects;
              static CEffectParam *pParam = PostEffectMgr().GetByName("WaterVolume_Amount"); 
              assert(pParam && "Parameter doesn't exist");

              // Activate puddle generation
              if( pParam )
                pParam->SetParam(1.0f);   

              CTexture::m_Text_WaterVolumeDDN->Apply(-1, nTS, nTexSlot, nSamplerSlot);
            }
            else
              CTexture::m_Text_FlatBump->Apply( -1 );


            //gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
            //CTexture::m_Text_WaterVolume->Apply(-1, nTS, nTexSlot, nSamplerSlot);
          }
          
          break;

        case TO_WATERPUDDLESMAP:
          {
            if( CTexture::m_Text_WaterPuddlesDDN )
            {
              using namespace NPostEffects;
              static CEffectParam *pParam = PostEffectMgr().GetByName("WaterPuddles_Amount"); 
              assert(pParam && "Parameter doesn't exist");

              // Activate puddle generation
              if( pParam )
                pParam->SetParam(1.0f);   

              CTexture::m_Text_WaterPuddlesDDN->Apply(-1, nTS, nTexSlot, nSamplerSlot);
            }
            else
              CTexture::m_Text_White->Apply( -1 );

          }
          break;


        case TO_TERRAIN_LM:
          {
            // do funky stuff
            static ICVar* e_shadows( iSystem->GetIConsole()->GetCVar( "e_shadows" ) );
            bool setupTerrainShadows( e_shadows->GetIVal() > 0 );

            if( setupTerrainShadows )
            {
              // terrain shadow map
              ITerrain* pTerrain( iSystem->GetI3DEngine()->GetITerrain() );
              Vec4 texGenInfo;
              int terrainLightMapID( pTerrain->GetTerrainLightmapTexId( texGenInfo ) );
              CTexture* pTerrainLightMap( terrainLightMapID > 0 ? CTexture::GetByID( terrainLightMapID ) : CTexture::m_Text_White );
              assert( pTerrainLightMap );

              STexState pTexStateLinearClamp;
              pTexStateLinearClamp.SetFilterMode(FILTER_LINEAR);        
              pTexStateLinearClamp.SetClampMode(true, true, true);
              int nTexStateLinearClampID = CTexture::GetTexState( pTexStateLinearClamp );

              pTerrainLightMap->Apply( -1, nTexStateLinearClampID, nTexSlot, nSamplerSlot );
            }
            else
            {
              CTexture::m_Text_White->Apply( -1 );
            }

            break;
          }

        case TO_CLOUDS_LM:
          {

            // do more funky stuff
            static ICVar* e_shadows( iSystem->GetIConsole()->GetCVar( "e_shadows" ) );
            static ICVar* e_shadows_clouds( iSystem->GetIConsole()->GetCVar( "e_shadows_clouds" ) );
            bool setupCloudShadows( e_shadows->GetIVal() > 0 && e_shadows_clouds->GetIVal() > 0 );

            if( setupCloudShadows )
            {
              // cloud shadow map 
              CTexture* pCloudShadowTex( rd->GetCloudShadowTextureId() > 0 ? CTexture::GetByID( rd->GetCloudShadowTextureId() ) : CTexture::m_Text_White );
              assert( pCloudShadowTex );

              STexState pTexStateLinearClamp;
              pTexStateLinearClamp.SetFilterMode(FILTER_LINEAR);        
              pTexStateLinearClamp.SetClampMode(false, false, false);
              int nTexStateLinearClampID = CTexture::GetTexState( pTexStateLinearClamp );

              pCloudShadowTex->Apply( -1, nTexStateLinearClampID, nTexSlot, nSamplerSlot );
            }
            else
            {
              CTexture::m_Text_White->Apply( -1 );
            }

            break;
          }

        case TO_MIPCOLORS_DIFFUSE:
          {
            CTexture *pTex = NULL;
            if (pSR && pSR->m_Textures[EFTT_DIFFUSE])
            {
              CTexture *tx = pSR->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex;
              pTex = CTexture::GenerateMipsColorMap(tx->GetWidth(), tx->GetHeight());
              if (pTex)
                pTex->Apply(-1);
            }
            if (!pTex)
              CTexture::m_Text_White->Apply(-1);
          }
          break;

        case TO_MIPCOLORS_BUMP:
          {
            CTexture *pTex = NULL;
            if (pSR && pSR->m_Textures[EFTT_BUMP])
            {
              CTexture *tx = pSR->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex;
              pTex = CTexture::GenerateMipsColorMap(tx->GetWidth(), tx->GetHeight());
              if (pTex)
                pTex->Apply(-1);
            }
            if (!pTex)
              CTexture::m_Text_White->Apply(-1);
          }
          break;

        case TO_BACKBUFFERMAP:            
          CTexture::GetBackBuffer(CTexture::m_Text_BackBuffer, 0);   
          CTexture::m_Text_BackBuffer->Apply(-1, nTS);    
          break;

#ifndef EXCLUDE_SCALEFORM_SDK
				case TO_FROMSF0:
				case TO_FROMSF1:
					{
						const SSF_GlobalDrawParams* pParams(rd->SF_GetGlobalDrawParams());
						assert(pParams);						
						int texIdx(nCustomID - TO_FROMSF0);
						int texID(pParams ? pParams->texture[texIdx].texID : -1);
						if (texID > 0)
						{
							const static int texStateID[8] = 
							{
								CTexture::GetTexState(STexState(FILTER_POINT, false)), CTexture::GetTexState(STexState(FILTER_POINT, true)),
								CTexture::GetTexState(STexState(FILTER_LINEAR, false)), CTexture::GetTexState(STexState(FILTER_LINEAR, true)),
								CTexture::GetTexState(STexState(FILTER_TRILINEAR, false)), CTexture::GetTexState(STexState(FILTER_TRILINEAR, true)),
								-1, -1
							};
							CTexture* pTex(CTexture::GetByID(texID));															
							int textStateID(texStateID[pParams->texture[texIdx].texState]);								
							pTex->Apply(-1, textStateID);
						}
						else
						{
							CTexture::m_Text_White->Apply(-1);
						}						
						break;
					}
#endif

			case TO_VOLOBJ_DENSITY:
			case TO_VOLOBJ_SHADOW:
				{
					bool texBound(false);					
					CRendElement* pRE(rd->m_RP.m_pRE);
					if (pRE && pRE->mfGetType() == eDATA_VolumeObject)
					{
						CREVolumeObject* pVolObj((CREVolumeObject*)pRE);						
						int texId(0);
						if (pVolObj)
						{
							switch(nCustomID)
							{
							case TO_VOLOBJ_DENSITY:
								if (pVolObj->m_pDensVol)
									texId = pVolObj->m_pDensVol->GetTexID();
								break;
							case TO_VOLOBJ_SHADOW:
								if (pVolObj->m_pShadVol)
									texId = pVolObj->m_pShadVol->GetTexID();
								break;
							default:
								assert(0);
								break;
							}
						}
						CTexture* pTex(texId > 0 ? CTexture::GetByID(texId) : 0);
						if (pTex)
						{
							pTex->Apply(-1, nTS, nTexSlot, nSamplerSlot);
							texBound = true;
						}
					}
					if (!texBound)
						CTexture::m_Text_White->Apply(-1);
					break;
				}

      default:
        {
          tx->Apply(-1, nTS, nTexSlot, nSamplerSlot);
        }
        break;
      }
    }
  }

  return true;
}

bool CHWShader_D3D::mfUpdateSamplers()
{
  if (!m_pCurInst)
    return false;
  SHWSInstance *pInst = m_pCurInst;
  if (!pInst->m_pSamplers)
    return true;

  SRenderShaderResources *pSR = gRenDev->m_RP.m_pShaderResources;
  uint i;
  const int nSize = pInst->m_pSamplers->size();
  STexSampler *pSamp = &(*pInst->m_pSamplers)[0];
  for (i=0; i<nSize; i++, pSamp++)
  {
    CTexture *tx = pSamp->m_pTex;
    assert(tx);
    if (!tx)
      continue;
    if (tx >= &CTexture::m_Templates[0] && tx <= &CTexture::m_Templates[EFTT_MAX-1])
    {
      if (tx < &CTexture::m_Templates[EFTT_LIGHTMAP] || tx > &CTexture::m_Templates[EFTT_OCCLUSION])
      {
        int nSlot = tx - &CTexture::m_Templates[0];
        if (pSR && pSR->m_Textures[nSlot])
        {
          if( nSlot != EFTT_DETAIL_OVERLAY || (nSlot == EFTT_DETAIL_OVERLAY && !(gRenDev->m_RP.m_pShader->m_Flags2 & EF2_DETAILBUMPMAPPING)))
            pSR->m_Textures[nSlot]->Update(nSlot);

          tx = pSR->m_Textures[nSlot]->m_Sampler.m_pTex;
//          if (nSlot == EFTT_BUMP)
//          {
//            if (pSR->m_Textures[EFTT_BUMP_HEIGHT])
//            {
//              assert(pSR->m_Textures[EFTT_BUMP_HEIGHT]->m_Sampler.m_pTex->GetFlags() & FT_ALPHA);
//            }
//          }
        }
        else
        {
          if (nSlot == EFTT_BUMP)
            tx = CTexture::m_Text_FlatBump;
        }
      }
    }
    if (tx)
    {
			if((tx->GetFlags()&FT_TEX_NORMAL_MAP)!=0)
			{
				if (tx->GetDstFormat() == eTF_DXT5)
					gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NORMAL_DXT5];
			}
    }
  }

  return true;
}

void CHWShader_D3D::mfGetSrcFileName(char *srcName, int nSize)
{
  if (!m_NameSourceFX.empty())
  {
    strncpy(srcName, m_NameSourceFX.c_str(), nSize);
    return;
  }
  strncpy(srcName, gRenDev->m_cEF.m_HWPath, nSize);
  if (m_eSHClass == eHWSC_Vertex)
    strncat(srcName, "Declarations/CGVShaders/", nSize);
  else
  if (m_eSHClass == eHWSC_Pixel)
    strncat(srcName, "Declarations/CGPShaders/", nSize);
  else
    strncat(srcName, "Declarations/CGGShaders/", nSize);
  strncat(srcName, GetName(), nSize);
  strncat(srcName, ".crycg", nSize);
}

void CHWShader_D3D::mfGenName(SHWSInstance *pInst, char *dstname, int nSize, byte bType)
{
  assert(dstname);
  dstname[0] = 0;

  char str[32];
  if (bType!=0 && pInst->m_GLMask)
  {
    sprintf(str, "(GL%x)", pInst->m_GLMask);
    strncat(dstname, str, nSize);
  }
  if (bType != 0)
  {
#if defined(__GNUC__)
    sprintf(str, "(RT%llx)", pInst->m_RTMask);
#else
    sprintf(str, "(RT%I64x)", pInst->m_RTMask);
#endif
    strncat(dstname, str, nSize);
  }
  if (bType!=0 && pInst->m_LightMask)
  {
    sprintf(str, "(LT%x)", pInst->m_LightMask);
    strncat(dstname, str, nSize);
  }
  if (bType!=0 && pInst->m_MDMask)
  {
    sprintf(str, "(MD%x)", pInst->m_MDMask);
    strncat(dstname, str, nSize);
  }
  if (bType!=0 && pInst->m_MDVMask)
  {
    sprintf(str, "(MDV%x)", pInst->m_MDVMask);
    strncat(dstname, str, nSize);
  }
  if (bType!=0 && !CParserBin::m_bD3D10)
  {
    sprintf(str, "(%s)", mfProfileToString(pInst->m_eProfileType));
    strncat(dstname, str, nSize);
  }
}

void CHWShader_D3D::mfGetDstFileName(SHWSInstance *pInst, CHWShader_D3D *pSH, char *dstname, int nSize, byte bType)
{
  strncpy(dstname, gRenDev->m_cEF.m_ShadersCache, nSize);
	//michaelg: Andrey: can we always use lower casing here?
	//PS3HACK : lower filename since it is checked for some directory strings to redirect to HOSTFS
#if defined(PS3)
	#define CGVSHADER_DIR "cgvshaders/"
	#define CGVSHADER_DEBUG_DIR "cgvshaders/debug/"
	#define CGVSHADER_PENDING_DIR "cgvshaders/pending/"
	#define CGPSHADER_DIR "cgpshaders/"
	#define CGPSHADER_DEBUG_DIR "cgpshaders/debug/"
	#define CGPSHADER_PENDING_DIR "cgpshaders/pending/"
#else
	#define CGVSHADER_DIR "CGVShaders/"
	#define CGVSHADER_DEBUG_DIR "CGVShaders/Debug/"
	#define CGVSHADER_PENDING_DIR "CGVShaders/Pending/"
	#define CGPSHADER_DIR "CGPShaders/"
	#define CGPSHADER_DEBUG_DIR "CGPShaders/Debug/"
	#define CGPSHADER_PENDING_DIR "CGPShaders/Pending/"
#endif

  if (pSH->m_eSHClass == eHWSC_Vertex)
  {
    if (bType == 1)
      strncat(dstname, CGVSHADER_DEBUG_DIR, nSize);
    else
    if (bType == 0)
      strncat(dstname, CGVSHADER_DIR, nSize);
    else
    if (bType >= 2)
      strncat(dstname, CGVSHADER_PENDING_DIR, nSize);
  }
  else
  if (pSH->m_eSHClass == eHWSC_Pixel)
  {
    if (bType == 1)
      strncat(dstname, CGPSHADER_DEBUG_DIR, nSize);
    else
    if (bType == 0)
      strncat(dstname, CGPSHADER_DIR, nSize);
    else
    if (bType >= 2)
      strncat(dstname, CGPSHADER_PENDING_DIR, nSize);
  }
  else
  if (pSH->m_eSHClass == eHWSC_Geometry)
  {
    if (bType == 1)
      strncat(dstname, "CGGShaders/Debug/", nSize);
    else
    if (bType == 0)
      strncat(dstname, "CGGShaders/", nSize);
    else
    if (bType == 2)
      strncat(dstname, "CGGShaders/Pending", nSize);
  }
  if (pSH->m_Flags & HWSG_SHARED)
  {
    char s[1024];
    sprintf(s, "_Shared@%s", pSH->m_EntryFunc.c_str());
    strncat(dstname, s, nSize);
    //strncat(dstname, GetName(), nSize);
  }
  else
    strncat(dstname, pSH->GetName(), nSize);
  if (bType == 2)
    strncat(dstname, "_out", nSize);
  if (bType == 0)
  {
    char *s = strchr(dstname, '(');
    if (s)
      s[0] = 0;
  }

  char szGenName[256];
  mfGenName(pInst, szGenName, 256, bType);

  strncat(dstname, szGenName, nSize);
}

//========================================================================================================
// Binary cache support

SShaderCache::~SShaderCache()
{
  CHWShader::m_ShaderCache.erase(m_Name);
  SAFE_DELETE(m_pRes[CACHE_USER]);
  SAFE_DELETE(m_pRes[CACHE_READONLY]);
}

int SShaderCache::Size()
{
  int nSize = sizeof(SShaderCache);

  if (m_pRes[0])
    nSize += m_pRes[0]->Size();
  if (m_pRes[1])
    nSize += m_pRes[1]->Size();
  nSize += m_DeviceShaders.size() * sizeof(SD3DShader);

  return nSize;
}

SShaderCache *CHWShader::mfInitCache(const char *name, CHWShader *pSH, bool bCheckValid, uint32 CRC32, bool bDontUseUserFolder, bool bReadOnly)
{
  CHWShader_D3D *pSHHW = (CHWShader_D3D *)pSH;
  char nameCache[256];
  if (!name)
  {
    char namedst[256];
    pSHHW->mfGetDstFileName(pSHHW->m_pCurInst, pSHHW, namedst, 256, 0);
    fpStripExtension(namedst, nameCache);
    fpAddExtension(nameCache, ".fxcb");
    name = nameCache;
  }

  SShaderCache *pCache = NULL;
  FXShaderCacheItor it = m_ShaderCache.find(name);
  if (it != m_ShaderCache.end())
  {
    pCache = it->second;
    pCache->m_nRefCount++;
    if (pSHHW)
    {
      if (bCheckValid)
      {
        int nCache[2] = {-1,-1};
        if (!bReadOnly || bDontUseUserFolder)
          nCache[0] = CACHE_USER;
        else
        if (!bDontUseUserFolder || bReadOnly)
        {
          nCache[0] = CACHE_USER;
          nCache[1] = CACHE_READONLY;
        }
        for (int i=0; i<2; i++)
        {
          if (nCache[i] < 0 || !pCache->m_pRes[i])
            continue;
          bool bValid = true;
          if (pSHHW->m_Flags & HWSG_SHARED)
            bValid = pSHHW->mfIsSharedCacheValid(pCache->m_pRes[i]);
          else
            bValid = (pCache->m_Header[i].m_CRC32 == CRC32);
          if (!bValid)
          {
            SAFE_DELETE(pCache->m_pRes[i]);
          }
        }
        bool bValid = true;
        if ((!bReadOnly || bDontUseUserFolder) && !pCache->m_pRes[CACHE_USER])
          bValid = false;
        else
        if ((!bDontUseUserFolder || bReadOnly) && !pCache->m_pRes[CACHE_READONLY] && !pCache->m_pRes[CACHE_USER])
          bValid = false;
        if (!bValid)
        {
          mfOpenCacheFile(name, (float)FX_CACHE_VER, pCache, pSH, bCheckValid, CRC32, bDontUseUserFolder, bReadOnly);
        }
      }
    }
  }
  else
  {
    pCache = new SShaderCache;
    pCache->m_bD3D10 = CParserBin::m_bD3D10;
    pCache->m_Name = name;
    mfOpenCacheFile(name, (float)FX_CACHE_VER, pCache, pSH, bCheckValid, CRC32, bDontUseUserFolder, bReadOnly);
    m_ShaderCache.insert(FXShaderCacheItor::value_type(name, pCache));
  }

  if (pSH && (pSH->m_Flags & HWSG_SHARED) && pCache->m_pRes[CACHE_USER])
    mfInsertSharedIdent(pCache, CRC32, pSH->m_NameSourceFX.c_str());

  return pCache;
}
SShaderCacheHeaderItem *CHWShader_D3D::mfGetCacheItem(uint nFlags, uint& nSize)
{
  SHWSInstance *pInst = m_pCurInst;
  nSize = 0;
  if (!m_pCache || (!m_pCache->m_pRes[CACHE_USER] && !m_pCache->m_pRes[CACHE_READONLY]))
    return NULL;
  CResFile *rf = NULL;
  SDirEntry *de = NULL;
  EHWSProfile ePr = pInst->m_eProfileType;
  int i;
  for (i=0; i<2; i++)
  {
    pInst->m_eProfileType = ePr;
    de = NULL;
    rf = m_pCache->m_pRes[i];
    if (!rf)
      continue;
    bool bIncr = false;
    CCryName nm;
    char name[128];
    mfGenName(pInst, name, 128, true);
    bool bFound = CCryName::find(name);
    if (bFound)
    {
      nm = CCryName(name);
      de = rf->mfGetEntry(nm);
      if (de)
        break;
      bIncr = true;
    }
    else
    {
#if defined (DIRECT3D10)
      return NULL;
#endif
      bIncr = true;
    }
    if (bIncr)
    {
      if (m_Flags & HWSG_PRECACHEPHASE)
        return NULL;
      while (mfNextProfile(nFlags))
      {
        mfGenName(pInst, name, 128, true);
        bFound = CCryName::find(name);
        if (bFound)
        {
          nm = CCryName(name);
          de = rf->mfGetEntry(nm);
          if (de)
            break;
        }
      }
      if (de)
        break;
      if (!bFound)
      {
        pInst->m_eProfileType = ePr;
        continue;
      }
    }
  }
  if (de)
  {
    pInst->m_nCache = i;
    nSize = rf->mfFileRead(de);
    void *pData = rf->mfFileGetBuf(de);
    if (!pData)
    {
      pInst->m_eProfileType = ePr;
      return NULL;
    }
    return (SShaderCacheHeaderItem *) pData;
  }
  else
  {
    pInst->m_eProfileType = ePr;
    return NULL;
  }
}

bool CHWShader_D3D::mfAddCacheItem(SShaderCache *pCache, SShaderCacheHeaderItem *pItem, const byte *pData, int nLen, bool bFlush, CCryName Name)
{
  if (!pCache)
    return false;
  if (!pCache->m_pRes[CACHE_USER])
    return false;

  pItem->m_CRC32 = GetCRC32Gen().GetCRC32((const char *)pData, nLen, 0xffffffff);
  byte *pNew = new byte[sizeof(SShaderCacheHeaderItem)+nLen];
  memcpy(pNew, pItem, sizeof(SShaderCacheHeaderItem));
  memcpy(&pNew[sizeof(SShaderCacheHeaderItem)], pData, nLen);
  SDirEntry de;
  de.Name = Name;
  de.earc = eARC_LZSS;
  de.size = nLen+sizeof(SShaderCacheHeaderItem);
  de.user.data = pNew;
  de.flags = RF_TEMPDATA;
  de.typeID = pItem->m_DeviceObjectID;
  pCache->m_pRes[CACHE_USER]->mfFileAdd(&de);
  mfMarkCacheOptimised(false, pCache);
  if (bFlush)
    pCache->m_pRes[CACHE_USER]->mfFlush();

  return true;
}

bool CHWShader::mfMarkCacheOptimised(bool bOptimised, SShaderCache *pCache)
{
  if (!pCache)
    return false;
  if (pCache->m_Header[CACHE_USER].m_bOptimised == bOptimised)
    return true;
  if (!pCache->m_pRes[CACHE_USER])
    return false;
  CResFile *pRes = pCache->m_pRes[CACHE_USER];

  pCache->m_Header[CACHE_USER].m_bOptimised = bOptimised;
  CCryName nmHead = CCryName("$HEAD");
  pRes->mfFileWrite(nmHead, &pCache->m_Header[CACHE_USER]);

  return true;
}

bool CHWShader_D3D::mfFreeCacheItem(SHWSInstance *pInst, SShaderCache *pCache)
{
  assert (pCache && pCache->m_pRes[pInst->m_nCache]);
  if (!pCache || !pCache->m_pRes[pInst->m_nCache])
    return false;
  CResFile *rf = pCache->m_pRes[pInst->m_nCache];
  char name[128];
  mfGenName(pInst, name, 128, true);
  bool bFound = CCryName::find(name);
  if (!bFound)
    return false;
  CCryName nm = CCryName(name);
  SDirEntry *de = rf->mfGetEntry(nm);
  if (!de)
    return false;
  SAFE_DELETE_VOID_ARRAY(de->user.data);
  return true;
}

bool CHWShader_D3D::mfFlushCacheFile()
{
  int i;

  for (i=0; i<m_Insts.Num(); i++)
  {
    SHWSInstance *pInst = &m_Insts[i];
    if (mfIsValid(pInst, false) ==  ED3DShError_Fake)
    {
      pInst->m_Handle.SetShader(NULL);
    }
  }
  return m_pCache && m_pCache->m_pRes[CACHE_USER] && m_pCache->m_pRes[CACHE_USER]->mfFlush();
}

struct SData
{
  CCryName Name;
  int nSize;
  uint CRC;
  byte *pData;
  byte bProcessed;
};
// Remove shader duplicates
bool CHWShader::mfOptimiseCacheFile(SShaderCache *pCache)
{
  if (pCache->m_Header[CACHE_USER].m_bOptimised)
    return true;
  CResFile *pRes = pCache->m_pRes[CACHE_USER];
  TArray<SDirEntry *> Dir;
  pRes->mfGetDirectory(Dir);
  std::vector<SData> Data;
  uint i, j;
  bool bNeedOptimise = false;
  for (i=0; i<Dir.Num(); i++)
  {
    SDirEntry *pDE = Dir[i];
    const char *str = pDE->GetName();
    if (str[0] == '$')
    {
      if (!strcmp(str, "$HEAD"))
        continue;
      SData d;
      d.nSize = pRes->mfFileRead(pDE);
      d.pData = new byte[d.nSize];
      memcpy(d.pData, pDE->user.data, d.nSize);
      d.bProcessed = 1;
      d.Name = pDE->Name;
      d.CRC = 0;
      Data.push_back(d);
      continue;
    }
    SData d;
    d.nSize = pRes->mfFileRead(pDE);
    d.pData = new byte[d.nSize];
    memcpy(d.pData, pDE->user.data, d.nSize);
    SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)d.pData;
    if (i && pItem->m_DeviceObjectID >= SH_UNIQ_ID)
      bNeedOptimise = true;
    d.bProcessed = 0;
    d.Name = pDE->Name;
    d.CRC = pItem->m_CRC32;
    Data.push_back(d);
  }
#ifdef DEBUG
  for (i=0; i<Data.size(); i++)
  {
    SData *pD1 = &Data[i];
    for (j=i+1; j<Data.size(); j++)
    {
      SData *pD2 = &Data[j];
      assert (pD1->Name != pD2->Name);
    }    
  }
#endif
  FILE *fp = NULL;
  int nDevID = 1;
  int nOutFiles = Data.size();
  if (bNeedOptimise)
  {
    for (i=0; i<Data.size(); i++)
    {
      if (fp)
      {
        iSystem->GetIPak()->FClose(fp);
        fp = NULL;
      }
      if (Data[i].bProcessed)
        continue;
      byte *pD = Data[i].pData;
      Data[i].bProcessed = 1;
      SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)pD;
      pItem->m_DeviceObjectID = nDevID++;
      byte *pSH = mfIgnoreBindsFromCache(pItem->m_nInstBinds, &pD[sizeof(SShaderCacheHeaderItem)]);
      int nSizeSh = Data[i].nSize - (pSH - pD);
      if (nSizeSh == 0)
        continue;
      assert(nSizeSh > 0);
      for (j=i+1; j<Data.size(); j++)
      {
        if (Data[j].bProcessed)
          continue;
        byte *pD1 = Data[j].pData;
        SShaderCacheHeaderItem *pItem1 = (SShaderCacheHeaderItem *)pD1;
        byte *pSH1 = mfIgnoreBindsFromCache(pItem1->m_nInstBinds, &pD1[sizeof(SShaderCacheHeaderItem)]);
        int nSizeSh1 = Data[j].nSize - (pSH1 - pD1);
        if (nSizeSh1 == 0)
        {
          Data[j].bProcessed = true;
          pItem1->m_DeviceObjectID = nDevID++;
          continue;
        }
        if (nSizeSh != nSizeSh1)
          continue;
        if (Data[i].CRC != Data[j].CRC)
          continue;
        if (!memcmp(pSH, pSH1, nSizeSh))
        {
          if (!memcmp(&pD[sizeof(SShaderCacheHeaderItem)], &pD1[sizeof(SShaderCacheHeaderItem)], pSH1 - pD1))
          {
            if (!fp && CRenderer::CV_r_shaderscacheoptimiselog)
            {
              char name[256];
              sprintf(name, "Optimise/%s/%s.cache", pRes->mfGetFileName(), Data[i].Name.c_str());
              fp = iSystem->GetIPak()->FOpen(name, "w");
            }
            pItem1->m_DeviceObjectID = pItem->m_DeviceObjectID;
            Data[j].bProcessed = 2;
            nOutFiles--;
            if (fp)
              iSystem->GetIPak()->FPrintf(fp, "%s\n", Data[j].Name.c_str());
          }
        }
      }
    }
  }
  if (fp)
  {
    iSystem->GetIPak()->FClose(fp);
    fp = NULL;
  }
  if (nOutFiles != Data.size())
  {
    iLog->Log(" Optimising shaders resource '%s' (%d items)...", pCache->m_Name.c_str(), Data.size()-1);

#ifdef _DEBUG
    for (i=0; i<Data.size(); i++)
    {
      SData *pD = &Data[i];
      assert(pD->bProcessed);
      SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)pD->pData;
      assert(pItem->m_DeviceObjectID < SH_UNIQ_ID);
    }
#endif

    pRes->mfClose();
    pRes->mfOpen(RA_CREATE);

    float fVersion = (float)FX_CACHE_VER;
    SDirEntry de;
    SShaderCacheHeader hd;
    ZeroStruct(hd);
    de.Name = CCryName("$HEAD");
    de.earc = eARC_NONE;
    de.size = sizeof(SShaderCacheHeader);
    de.user.data = &hd;
    hd.m_SizeOf = sizeof(SShaderCacheHeader);
    hd.m_MinorVer = (int)(((float)fVersion - (float)(int)fVersion)*10.1f);
    hd.m_MajorVer = (int)fVersion;
    hd.m_CRC32 = pCache->m_Header[CACHE_USER].m_CRC32;
    sprintf(hd.m_szVer, "Ver: %.1f", fVersion);
    pRes->mfFileAdd(&de);
    pRes->mfFlush();

    for (i=0; i<Data.size(); i++)
    {
      SData *pD = &Data[i];
      SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)pD->pData;

      SDirEntry de;
      de.Name = pD->Name;
      de.earc = eARC_LZSS;
      de.size = pD->nSize;
      if (pD->bProcessed == 1)
      {
        byte *pNew = new byte[de.size];
        memcpy(pNew, pD->pData, pD->nSize);
        de.user.data = pNew;
        de.refOrg = pItem->m_DeviceObjectID;
      }
      else
        de.ref = pItem->m_DeviceObjectID;
      de.typeID = pItem->m_DeviceObjectID;
      de.flags = RF_TEMPDATA;
      pRes->mfFileAdd(&de);
    }
  }

  for (i=0; i<Data.size(); i++)
  {
    SData *pD = &Data[i];
    delete [] pD->pData;
  }
  if (nOutFiles != Data.size())
    iLog->Log("  -- Removed %d duplicated shaders", Data.size()-nOutFiles);

  float fVersion = (float)FX_CACHE_VER;
  sprintf(pCache->m_Header[CACHE_USER].m_szVer, "Ver: %.1f", fVersion);

  Data.clear();
  mfMarkCacheOptimised(true, pCache);
  pRes->mfFlush();

  return true;
}

bool CHWShader::mfIsSharedCacheValid(CResFile *pRF)
{
  TArray<SDirEntry *> Dir;
  pRF->mfGetDirectory(Dir);
  char dstname[256];
  uint i;
  for (i=0; i<Dir.Num(); i++)
  {
    SDirEntry *pDE = Dir[i];
    const char *n = pDE->GetName();
    if (n[0] != '$')
      continue;
    if (!strcmp(n, "$HEAD"))
      continue;
    sprintf(dstname, "%s.cfx", &n[1]);
    FXShaderBinPathItor it = gRenDev->m_cEF.m_Bin.m_BinPaths.find(CCryName(dstname));
    if (it == gRenDev->m_cEF.m_Bin.m_BinPaths.end())
    {
      assert(0);
      return false;
    }
    const char *szBinPath = it->second.c_str();
    FILE *fp = iSystem->GetIPak()->FOpen(szBinPath, "rb");
    if (!fp)
    {
      assert(0);
      return false;
    }
    SShaderBinHeader Header;
    iSystem->GetIPak()->FRead((byte *)&Header, sizeof(Header), fp);
    uint32 CRC32 = Header.m_CRC32;
    pRF->mfFileRead(pDE);
    byte *pData = (byte *)pRF->mfFileGetBuf(pDE);
    uint32 CRC32Cache = *(uint32 *)&pData[0];
    iSystem->GetIPak()->FClose(fp);
    if (CRC32 != CRC32Cache)
      return false;
  }
  return true;
}

bool CHWShader::mfInsertSharedIdent(SShaderCache *pCache, uint32 CRC32, const char *szNameFX)
{
  if (!pCache)
    return false;
  TArray<SDirEntry *> Dir;
  CResFile *pRF = pCache->m_pRes[CACHE_USER];
  if(!pRF)//happened on PS3 
    return false;
  pRF->mfGetDirectory(Dir);
  uint i;

  char nm[256];
  nm[0] = 0;
  _splitpath(szNameFX, NULL, NULL, nm, NULL);
  if (!nm[0])
    return false;
  for (i=0; i<Dir.Num(); i++)
  {
    SDirEntry *pDE = Dir[i];
    const char *n = pDE->GetName();
    if (n[0] != '$')
      continue;
    if (!stricmp(&n[1], nm))
      return true;
  }
  byte *pNewData = new byte [sizeof(uint32)];
  *(uint32 *)pNewData = CRC32;
  SDirEntry de;
  char name[256];
  sprintf(name, "$%s", nm);
  de.Name = CCryName(name);
  de.earc = eARC_NONE;
  de.size = sizeof(uint32);
  de.user.data = pNewData;
  de.flags = RF_TEMPDATA;
  pRF->mfFileAdd(&de);

  return true;
}

bool CHWShader::_OpenCacheFile(float fVersion, SShaderCache *pCache, CHWShader *pSH, bool bCheckValid, uint32 CRC32, int nCache, CResFile *pRF, bool bReadOnly)
{
  SShaderCacheHeader hd;
  ZeroStruct(hd);
  bool bValid = true;
  CHWShader_D3D *pSHHW = (CHWShader_D3D *)pSH;

  if (!pRF->mfOpen(RA_READ))
  {
    pRF->mfClose();
    bValid = false;
  }
  else
  {
    if (pSH)
    {
      if (pSH->m_Flags & HWSG_SHARED)
      {
        if (bCheckValid)
          bValid = mfIsSharedCacheValid(pRF);
      }
      if (!bValid && CRenderer::CV_r_shadersdebug==2)
      {
        LogWarning("WARNING: Shader cache shared '%s' mismatch", pRF->mfGetFileName());
      }
    }
    if (bValid)
    {
      pRF->mfFileSeek("$HEAD", 0, SEEK_SET);
      pRF->mfFileRead2("$HEAD", sizeof(SShaderCacheHeader), &hd);
      if (bCheckValid)
      {
        if (hd.m_SizeOf != sizeof(SShaderCacheHeader))
          bValid = false;
        else
        if (fVersion && (hd.m_MajorVer != (int)fVersion || hd.m_MinorVer != (int)(((float)fVersion - (float)(int)fVersion)*10.1f)))
          bValid = false;
        if (!bValid && CRenderer::CV_r_shadersdebug==2)
        {
          LogWarning("WARNING: Shader cache '%s' version mismatch (Cache: %s, Current: %.1f)", pRF->mfGetFileName(), hd.m_szVer, fVersion);
        }
        if (pSH)
        {
          if (bValid && hd.m_CRC32 != pSHHW->m_CRC32)
          {
            if (!(pSH->m_Flags & HWSG_SHARED))
            {
              bValid = false;
              if (CRenderer::CV_r_shadersdebug==2)
              {
                LogWarning("WARNING: Shader cache '%s' CRC mismatch", pRF->mfGetFileName());
              }
            }
          }
        }
      }
    }

    if (nCache == CACHE_USER)
    {
      pRF->mfClose();
      if (bValid)
      {
        if (!pRF->mfOpen(RA_READ|RA_WRITE))
        {
          pRF->mfClose();
          bValid = false;
        }
      }
    }
  }
  if (!bValid && bCheckValid)
  {
    if (nCache == CACHE_USER && !bReadOnly)
    {
      if (!pRF->mfOpen(RA_CREATE))
        return false;

      SDirEntry de;
      de.Name = CCryName("$HEAD");
      de.earc = eARC_NONE;
      de.size = sizeof(SShaderCacheHeader);
      de.user.data = &hd;
      hd.m_SizeOf = sizeof(SShaderCacheHeader);
      hd.m_MinorVer = (int)(((float)fVersion - (float)(int)fVersion)*10.1f);
      hd.m_MajorVer = (int)fVersion;
      hd.m_CRC32 = CRC32;
      hd.m_bOptimised = false;
      sprintf(hd.m_szVer, "Ver: %.1f", fVersion);
      pRF->mfFileAdd(&de);
      if (pSHHW)
        pRF->mfFlush();
      pCache->m_bNeedPrecache = true;
      pCache->m_nDeviceShadersCounterShared = SH_SHARED_ID;
      bValid = true;
    }
    else
    {
      SAFE_DELETE(pRF);
    }
  }
  pCache->m_pRes[nCache] = pRF;
  pCache->m_Header[nCache] = hd;
  pCache->m_bReadOnly[nCache] = bReadOnly;

  return bValid;
}

bool CHWShader::mfOpenCacheFile(const char *szName, float fVersion, SShaderCache *pCache, CHWShader *pSH, bool bCheckValid, uint32 CRC32, bool bDontUseUserFolder, bool bReadOnly)
{
  CResFile *rfRO = new CResFile(szName);
  bool bValidRO = _OpenCacheFile(fVersion, pCache, pSH, bCheckValid, CRC32, bDontUseUserFolder ? CACHE_USER : CACHE_READONLY, rfRO, bDontUseUserFolder ? false : bReadOnly);

  bool bValidUser = false;
  CResFile *rfUser;
  if (!bDontUseUserFolder)
  {
    string szUser = gRenDev->m_cEF.m_szUserPath + string(szName);
    rfUser = new CResFile(szUser.c_str());
    bValidUser = _OpenCacheFile(fVersion, pCache, pSH, bCheckValid, CRC32, CACHE_USER, rfUser, bReadOnly);
  }

  int nMaxRef = SH_SHARED_ID;
  TArray<SDirEntry *> Dir;
  if (bValidRO)
  {
    rfRO->mfGetDirectory(Dir);
    for (int i=0; i<Dir.Num(); i++)
    {
      SDirEntry *pE = Dir[i];
      if (pE->typeID >= SH_SHARED_ID && pE->typeID < SH_UNIQ_ID)
        nMaxRef = max(nMaxRef, pE->typeID+1);
    }
  }
  if (bValidUser)
  {
    Dir.SetUse(0);
    rfUser->mfGetDirectory(Dir);
    for (int i=0; i<Dir.Num(); i++)
    {
      SDirEntry *pE = Dir[i];
      if (pE->typeID >= SH_SHARED_ID && pE->typeID < SH_UNIQ_ID)
        nMaxRef = max(nMaxRef, pE->typeID+1);
    }
  }
  pCache->m_nDeviceShadersCounterShared = nMaxRef;

  return (bValidRO || bValidUser);
}

byte *CHWShader_D3D::mfBindsToCache(SHWSInstance *pInst, DynArray<SCGBind>* Binds, int nParams, byte *pP)
{
  int i;
  for (i=0; i<nParams; i++)
  {
    SCGBind *cgb = &(*Binds)[i];
    SShaderCacheHeaderItemVar *pVar = (SShaderCacheHeaderItemVar *)pP;
    pVar->m_nCount = cgb->m_nParameters;
    pVar->m_Reg = cgb->m_dwBind;
    int len = strlen(cgb->m_Name.c_str())+1;
    memcpy(pVar->m_Name,  cgb->m_Name.c_str(), len);
    pP += sizeof(SShaderCacheHeaderItemVar)-MAX_VAR_NAME+len;
  }
  return pP;
}

byte *CHWShader_D3D::mfBindsFromCache(DynArray<SCGBind>*& Binds, int nParams, byte *pP)
{
  int i;
  for (i=0; i<nParams; i++)
  {
    if (!Binds)
      Binds = new DynArray<SCGBind>;
    SCGBind cgb;
    SShaderCacheHeaderItemVar *pVar = (SShaderCacheHeaderItemVar *)pP;
    cgb.m_nParameters = pVar->m_nCount;
    cgb.m_Name = pVar->m_Name;
    cgb.m_dwBind = pVar->m_Reg;
    Binds->push_back(cgb);
    pP += sizeof(SShaderCacheHeaderItemVar)-MAX_VAR_NAME+strlen(pVar->m_Name)+1;
  }
  return pP;
}

byte *CHWShader::mfIgnoreBindsFromCache(int nParams, byte *pP)
{
  int i;
  for (i=0; i<nParams; i++)
  {
    SShaderCacheHeaderItemVar *pVar = (SShaderCacheHeaderItemVar *)pP;
    pP += sizeof(SShaderCacheHeaderItemVar)-MAX_VAR_NAME+strlen(pVar->m_Name)+1;
  }
  return pP;
}

bool CHWShader_D3D::mfUploadHW(SHWSInstance *pInst, byte *pBuf, uint nSize)
{
  PROFILE_FRAME(Shader_mfUploadHW);

  HRESULT hr = S_OK;
  if (!pInst->m_Handle.m_pShader)
    pInst->m_Handle.SetShader(new SD3DShader);
#ifdef WIN32
  if(CRenderer::m_pDirectBee)
    CRenderer::m_pDirectBee->PushName(GetName());
#endif
#if defined (DIRECT3D9) || defined (OPENGL)
# if defined(PS3) && defined(_DEBUG)
  // Pass the shader name (for debugging only).
  { char nameSuffix[256];
  mfGenName(pInst, nameSuffix, sizeof nameSuffix - 1, true);
  nameSuffix[sizeof nameSuffix - 1] = 0;
  snprintf(ps3ShaderName, ps3ShaderName_size - 1,
    "%s%s", GetName(), nameSuffix);
  ps3ShaderName[ps3ShaderName_size - 1] = 0;
  }
# endif
  if (m_eSHClass == eHWSC_Pixel)
    hr = gcpRendD3D->GetD3DDevice()->CreatePixelShader((DWORD*)pBuf, (IDirect3DPixelShader9 **)&pInst->m_Handle.m_pShader->m_pHandle);
  else
    hr = gcpRendD3D->GetD3DDevice()->CreateVertexShader((DWORD*)pBuf, (IDirect3DVertexShader9 **)&pInst->m_Handle.m_pShader->m_pHandle);

# if defined(PS3) && defined(_DEBUG)
  ps3ShaderName[0] = 0;
# endif
#elif defined (DIRECT3D10)
  if (m_eSHClass == eHWSC_Pixel)
    hr = gcpRendD3D->GetD3DDevice()->CreatePixelShader((DWORD*)pBuf, nSize, (ID3D10PixelShader **)&pInst->m_Handle.m_pShader->m_pHandle);
  else
  if (m_eSHClass == eHWSC_Vertex)
    hr = gcpRendD3D->GetD3DDevice()->CreateVertexShader((DWORD*)pBuf, nSize, (ID3D10VertexShader **)&pInst->m_Handle.m_pShader->m_pHandle);
  else
  if (m_eSHClass == eHWSC_Geometry)
    hr = gcpRendD3D->GetD3DDevice()->CreateGeometryShader((DWORD*)pBuf, nSize, (ID3D10GeometryShader **)&pInst->m_Handle.m_pShader->m_pHandle);
#endif

#ifdef WIN32
  if(CRenderer::m_pDirectBee)
    CRenderer::m_pDirectBee->PushName();
#endif

  return (hr == S_OK);
}

bool CHWShader_D3D::mfUploadHW(LPD3DXBUFFER pShader)
{
  bool bResult = true;
  SHWSInstance *pInst = m_pCurInst;
  if (pShader && !(m_Flags & HWSG_PRECACHEPHASE))
  {
    DWORD *pCode = (DWORD*)pShader->GetBufferPointer();
#ifdef OPENGL
    CGprogram Prog = (CGprogram)pCode;
# if defined(PS3) && !defined(CRY_USE_GCM)
    // FIXME
    // Temporary workaround for a bug/limitation in the Cell SDK PSGL
    // library.
    pCode = (DWORD *)GetShaderProgram(Prog);
# else
    pCode = (DWORD *)cgGetProgramString(Prog, CG_COMPILED_PROGRAM);
# endif
#endif
    //if (gcpRendD3D->CheckDeviceLost())
    //  mfLostDevice(pInst, (byte *)pCode, pShader->GetBufferSize());
    //else
    if (gcpRendD3D->m_cEF.m_nCombinations>0 && !gcpRendD3D->m_cEF.m_bActivatePhase)
    {
      pInst->m_Handle.SetFake();
    }
    else
    {
      bResult = mfUploadHW(pInst, (byte *)pCode, pShader->GetBufferSize());
#if defined (DIRECT3D10)
      if (m_eSHClass == eHWSC_Vertex)
      {
        int nSize = pShader->GetBufferSize();
        pInst->m_pShaderData = new byte[nSize];
        pInst->m_nShaderByteCodeSize = nSize;
        memcpy(pInst->m_pShaderData, pCode, nSize);
      }
#endif
    }
    if (!bResult)
    {
      if (m_eSHClass == eHWSC_Vertex)
        Warning("CHWShader_D3D::mfUploadHW: Could not create vertex shader '%s'(0x%x)\n", GetName(), pInst->m_GLMask);
      else
      if (m_eSHClass == eHWSC_Pixel)
        Warning("CHWShader_D3D::mfUploadHW: Could not create pixel shader '%s'(0x%x)\n", GetName(), pInst->m_GLMask);
      else
      if (m_eSHClass == eHWSC_Geometry)
        Warning("CHWShader_D3D::mfUploadHW: Could not create geometry shader '%s'(0x%x)\n", GetName(), pInst->m_GLMask);
    }
  }
  return bResult;
}


bool CHWShader_D3D::mfActivateCacheItem(SShaderCacheHeaderItem *pItem, uint nSize)
{
  SHWSInstance *pInst = m_pCurInst;
  byte *pData = (byte *)pItem;
  pData += sizeof(SShaderCacheHeaderItem);
  byte *pBuf = pData;
  DynArray<SCGBind> *pInstBinds = NULL;
  pInst->Release(m_pCache);
  pBuf = mfBindsFromCache(pInstBinds, pItem->m_nInstBinds, pBuf);
  nSize -= pBuf - (byte *)pItem;
  pInst->m_eProfileType = (EHWSProfile)pItem->m_Profile;
  pInst->m_nVertexFormat = pItem->m_nVertexFormat;
  pInst->m_nInstructions = pItem->m_nInstructions;
  pInst->m_DeviceObjectID = pItem->m_DeviceObjectID;
  assert(pInst->m_DeviceObjectID > 0);
  pInst->m_VStreamMask_Decl = pItem->m_StreamMask_Decl;
  pInst->m_VStreamMask_Stream = pItem->m_StreamMask_Stream;
  bool bResult = true;
  SD3DShader *pHandle = NULL;
  SShaderCache *pCache = m_pCache;
  if (pItem->m_DeviceObjectID < SH_UNIQ_ID)
  {
    FXDeviceShaderItor it = pCache->m_DeviceShaders.find(pInst->m_DeviceObjectID);
    if (it != pCache->m_DeviceShaders.end())
      pHandle = it->second;
  }
  if (pHandle)
  {
    pInst->m_Handle.SetShader(pHandle);
    pInst->m_Handle.AddRef();
  }
  else
  {
    if (gcpRendD3D->m_cEF.m_nCombinations>0 && !gcpRendD3D->m_cEF.m_bActivatePhase)
    {
      pInst->m_Handle.SetFake();
    }
    else
    {
      bResult = mfUploadHW(pInst, pBuf, nSize);
    }
    if (!bResult)
    {
      SAFE_DELETE(pInstBinds);
      assert(!"Shader creation error");
      return true;
    }
    pCache->m_DeviceShaders.insert(FXDeviceShaderItor::value_type(pInst->m_DeviceObjectID, pInst->m_Handle.m_pShader));
  }
  LPD3DXCONSTANTTABLE pConstantTable = NULL;
#if defined (DIRECT3D9) || defined (OPENGL)
  HRESULT hr = D3DXGetShaderConstantTable((DWORD *)pBuf, &pConstantTable);
#elif defined (DIRECT3D10)
  ID3D10ShaderReflection *pShaderReflection;
  HRESULT hr = D3D10ReflectShader(pBuf, nSize, &pShaderReflection);
  if (SUCCEEDED(hr))
    pConstantTable = (LPD3DXCONSTANTTABLE)pShaderReflection;	
  if (m_eSHClass == eHWSC_Vertex || gRenDev->m_bEditor)
  {
    pInst->m_pShaderData = new byte[nSize];
    pInst->m_nShaderByteCodeSize = nSize;
    memcpy(pInst->m_pShaderData, pBuf, nSize);
  }
#endif
  assert(hr == S_OK);
  bResult &= (hr == S_OK);
  if (pConstantTable)
    mfCreateBinds(pInst, pConstantTable, pBuf, nSize);
#ifdef OPENGL
  int i;
  if (pInst->m_pBindVars)
  {
    for (i=0; i<pInst->m_pBindVars->size(); i++)
    {
      D3DXGetSHParamHandle(pInst->m_Handle.m_pHandle, &(*pInst->m_pBindVars)[i]);
    }
  }
  if (pInstBinds)
  {
    for (i=0; i<pInstBinds->size(); i++)
    {
      D3DXGetSHParamHandle(pInst->m_Handle.m_pHandle, &(*pInstBinds)[i]);
    }
  }
#endif

  mfGatherFXParameters(pInst, pInst->m_pBindVars, pInstBinds, this, 0);
  SAFE_DELETE(pInstBinds);
#if defined (DIRECT3D9) || defined (OPENGL)
  SAFE_RELEASE(pConstantTable);
#elif defined (DIRECT3D10)
  SAFE_RELEASE(pShaderReflection);
#endif

  return bResult;
}

bool CHWShader_D3D::mfCreateCacheItem(SHWSInstance *pInst, DynArray<SCGBind>& InstBinds, byte *pData, int nLen, int nDevObjectID, CHWShader_D3D *pSH, bool bShaderThread)
{
  if (!pSH->m_pCache || !pSH->m_pCache->m_pRes[CACHE_USER])
    pSH->m_pCache = mfInitCache(NULL, pSH, true, pSH->m_CRC32, false, false);
  assert(pSH->m_pCache);
  if (!pSH->m_pCache || !pSH->m_pCache->m_pRes[CACHE_USER])
    return false;

  DynArray<DWORD> NewData;
  bool bNeedConvert = false;
#if defined (DIRECT3D9) || defined (PS3)
  if (CParserBin::m_bD3D10)
    bNeedConvert = true;
#elif defined (DIRECT3D10)
  if (!CParserBin::m_bD3D10)
    bNeedConvert = true;
#endif
  if (bNeedConvert && pData)
  {
    char *sSrc = (char *)pData;
    
    char *sS = strstr(sSrc, "// Approximately");
    if (sS)
    {
      pInst->m_nInstructions = atoi(&sS[17]);
      sS = strstr(sS, "const DWORD g_");
    }
    assert(sS);
    sS = strchr(sS, '{');
    assert(sS);
    sS++;
    while(true)
    {
      char val[16];
      shFill(&sS, val, 16);
      if (val[0] != '0' && val[1] != 'x')
        break;
      DWORD dw = shGetHex(&val[2]);
      NewData.push_back(dw);
    }

    pData = (byte *)&NewData[0];
    nLen = NewData.size()*sizeof(DWORD);
  }


  SShaderCacheHeaderItem h;
  h.m_nInstBinds = InstBinds.size();
  h.m_nInstructions = pInst->m_nInstructions;
  h.m_nVertexFormat = pInst->m_nVertexFormat;
  h.m_Profile = pData ? pInst->m_eProfileType : 255;
  h.m_StreamMask_Decl = pInst->m_VStreamMask_Decl;
  h.m_StreamMask_Stream = pInst->m_VStreamMask_Stream;
  h.m_DeviceObjectID = nDevObjectID>0 ? nDevObjectID : pSH->m_pCache->m_nDeviceShadersCounterUniq++;
#ifdef OPENGL
  CGprogram Prog = (CGprogram)pData;
  if (!Prog)
    return false;
#if defined(PS3) && !defined(CRY_USE_GCM)
  // FIXME
  // Temporary workaround for a bug/limitation in the Cell SDK PSGL
  // library.
  { 
    size_t size;
    pData = (byte*)GetShaderProgram(Prog, size);
    nLen = size;
  }
#else
  pData = (byte *)cgGetProgramString(Prog, CG_COMPILED_PROGRAM);
  nLen = strlen((char *)pData)+1;
#endif
#endif
  int nNewSize = (h.m_nInstBinds)*sizeof(SShaderCacheHeaderItemVar)+nLen;
  byte *pNewData = new byte [nNewSize];
  byte *pP = pNewData;
  pP = mfBindsToCache(pInst, &InstBinds, h.m_nInstBinds, pP);
  memcpy(pP, pData, nLen);
  pP += nLen;
  char name[256];
  mfGenName(pInst, name, 256, true);
  CCryName nm = CCryName(name);
  bool bRes = mfAddCacheItem(pSH->m_pCache, &h, pNewData, pP-pNewData, false, nm);
  SAFE_DELETE_ARRAY(pNewData);
  if (gRenDev->m_cEF.m_bActivatePhase || (!(pSH->m_Flags & HWSG_PRECACHEPHASE) && gRenDev->m_cEF.m_nCombinations <= 0))
  {
    if (!gRenDev->m_cEF.m_bActivatePhase)
    {
#if !defined(XENON) && !defined(PS3)
      if (bShaderThread && false)
      {
        if (pInst->m_pAsync)
          pInst->m_pAsync->m_bPendedFlush = true;
      }
      else
#endif
        pSH->mfFlushCacheFile();
    }
    strcpy(name, pSH->GetName());
    char *s = strchr(name, '(');
    if (s)
      s[0] = 0;
    if (!bShaderThread || true)
      gRenDev->m_cEF.mfInsertNewCombination(pSH->m_nMaskGenFX, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, name);
  }
  pInst->m_nCache = CACHE_USER;

  return bRes;
}

//============================================================================

bool CHWShader_D3D::mfSetHWStartProfile(uint nFlags)
{
  CD3D9Renderer *rd = gcpRendD3D;
  SHWSInstance *pInst = m_pCurInst;
  if (m_eSHClass == eHWSC_Pixel)
  {
    EHWSProfile prMin = eHWSP_PS_2_0;
    EHWSProfile pr20 = (rd->GetFeatures() & RFT_HW_MASK) == RFT_HW_GFFX ? eHWSP_PS_2_X : eHWSP_PS_2_0;
    if (CParserBin::m_bD3D10 || (m_Flags & HWSG_40ONLY))
    {
#if defined(PS3)
      pInst->m_eProfileType = eHWSP_PS_3_0;
#else
      pInst->m_eProfileType = eHWSP_PS_4_0;
#endif
    }
    else
    {
      int64 nDebugMask = (g_HWSR_MaskBit[HWSR_DEBUG0] | g_HWSR_MaskBit[HWSR_DEBUG1] | g_HWSR_MaskBit[HWSR_DEBUG2] | g_HWSR_MaskBit[HWSR_DEBUG3]);
      if ((pInst->m_RTMask & nDebugMask) && (pInst->m_RTMask & nDebugMask) != nDebugMask)
      {
        if (rd->m_Features & RFT_HW_PS30)
          pInst->m_eProfileType = eHWSP_PS_3_0;
        else
          return false;
      }
      else
      if (m_Flags & HWSG_30ONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS30)
          pInst->m_eProfileType = eHWSP_PS_3_0;
        else
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = pr20;
        else
          pInst->m_eProfileType = prMin;
      }
      else
      if (m_Flags & HWSG_2XONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS2X)
          pInst->m_eProfileType = eHWSP_PS_2_X;
        else
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = pr20;
        else
          pInst->m_eProfileType = prMin;
      }
      else
      if (m_Flags & HWSG_20ONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = pr20;
        else
          pInst->m_eProfileType = prMin;
      }
      else
        pInst->m_eProfileType = prMin;
    }
  }
  else
  if (m_eSHClass == eHWSC_Vertex)
  {
		EHWSProfile prMin = eHWSP_VS_2_0;
    if (CParserBin::m_bD3D10 || (m_Flags & HWSG_40ONLY))
    {
#if defined(PS3)
      pInst->m_eProfileType = eHWSP_VS_3_0;
#else
      pInst->m_eProfileType = eHWSP_VS_4_0;
#endif
    }
    else
    {
      int64 nDebugMask = (g_HWSR_MaskBit[HWSR_DEBUG0] | g_HWSR_MaskBit[HWSR_DEBUG1] | g_HWSR_MaskBit[HWSR_DEBUG2] | g_HWSR_MaskBit[HWSR_DEBUG3]);
      if ((pInst->m_RTMask & nDebugMask) && (pInst->m_RTMask & nDebugMask) != nDebugMask)
      {
        if (rd->m_Features & RFT_HW_PS30)
          pInst->m_eProfileType = eHWSP_VS_3_0;
        else
          return false;
      }
      else
      if (m_Flags & HWSG_30ONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS30)
          pInst->m_eProfileType = eHWSP_VS_3_0;
        else
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = eHWSP_VS_2_0;
        else
          pInst->m_eProfileType = prMin;
      }
      else
      if (m_Flags & HWSG_2XONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = eHWSP_VS_2_0;
        else
          pInst->m_eProfileType = eHWSP_VS_1_1;
      }
      else
      if (m_Flags & HWSG_20ONLY)
      {
        if (rd->GetFeatures() & RFT_HW_PS20)
          pInst->m_eProfileType = eHWSP_VS_2_0;
        else
          pInst->m_eProfileType = prMin;
      }
      else
        pInst->m_eProfileType = prMin;
    }
  }
  else
  if (m_eSHClass == eHWSC_Geometry)
  {
    pInst->m_eProfileType = eHWSP_GS_4_0;
  }
  else
  {
    assert(0);
  }
  return true;
}

void CHWShader_D3D::mfSaveCGFile(const char *scr, const char *path)
{
  if (CRenderer::CV_r_shadersdebug < 1 && CRenderer::CV_r_shadersuserfolder)
    return;
  char name[1024];
  if (path && path[0])
  {
#ifdef XENON
    sprintf(name, "%s/%s(LT%x)/(RT%I64x)(MD%x)(MDV%x)(GL%x).cg", path, GetName(), m_pCurInst->m_LightMask, m_pCurInst->m_RTMask, m_pCurInst->m_MDMask, m_pCurInst->m_MDVMask, m_pCurInst->m_GLMask);
#else
#if defined(__GNUC__)
    sprintf(name, "%s/%s(LT%x)@(RT%llx)(MD%x)(MDV%x)(GL%x).cg", path, GetName(), m_pCurInst->m_LightMask, m_pCurInst->m_RTMask, m_pCurInst->m_MDMask, m_pCurInst->m_MDVMask, m_pCurInst->m_GLMask);
#else
    sprintf(name, "%s/%s(LT%x)/(RT%I64x)(MD%x)(MDV%x)(GL%x).cg", path, GetName(), m_pCurInst->m_LightMask, m_pCurInst->m_RTMask, m_pCurInst->m_MDMask, m_pCurInst->m_MDVMask, m_pCurInst->m_GLMask);
#endif
#endif
  }
  else
  {
#if defined(__GNUC__)
		//PS3HACK
    sprintf(name, "Shaders/Cache/D3D10/fxerror/%s(GL%x)@(LT%x)(RT%llx)@(MD%x)(MDV%x).cg", GetName(), m_pCurInst->m_GLMask, m_pCurInst->m_LightMask, m_pCurInst->m_RTMask, m_pCurInst->m_MDMask, m_pCurInst->m_MDVMask);
#else
    sprintf(name, "FXError/%s(GL%x)/(LT%x)(RT%I64x)/(MD%x)(MDV%x).cg", GetName(), m_pCurInst->m_GLMask, m_pCurInst->m_LightMask, m_pCurInst->m_RTMask, m_pCurInst->m_MDMask, m_pCurInst->m_MDVMask);
#endif
  }
  FILE *fp = iSystem->GetIPak()->FOpen(name, "w");
  if (fp)
  {
    iSystem->GetIPak()->FPrintf(fp, "%s",scr);
    iSystem->GetIPak()->FClose (fp);
  }
}

void CHWShader_D3D::mfOutputCompilerError(string& strErr, const char *szSrc)
{
  if (CRenderer::CV_r_shadersdebug)
  {
    FILE *fp = fopen("$$err", "w");
    if (!fp)
      return;
    fputs(szSrc, fp);
    fclose (fp);
  }

  string strE;
  strE.Format("FX %s shader '%s' compilation error:\n", m_eSHClass == eHWSC_Vertex ? "Vertex" : "Pixel", GetName());
  strE += strErr;
  while(true)
  {
    if (strE.find("$$in.cg") == string::npos)
      break;
    strE.replace("$$in.cg", "$$err");
  }
  OutputDebugString(strE.c_str());
  Warning( strE.c_str() );
}

#ifdef WIN32
bool CHWShader_D3D::mfPostCompiling(const char *szNameSrc, const char *szNameDst, LPD3DXBUFFER* ppShader, LPD3DXCONSTANTTABLE *ppConstantTable)
{
  SHWSInstance *pInst = m_pCurInst;

  FILE *fp = fopen(szNameDst, "rb");
  if (!fp)
  {
    //assert(0);
    remove(szNameSrc);
    return false;
  }
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (size < 20)
  {
    //assert(0);
    fclose(fp);
    remove(szNameSrc);
    remove(szNameDst);
    return false;
  }

  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined(OPENGL)
  hr = D3DXCreateBuffer(size, ppShader);
  LPD3DXBUFFER pShader = *ppShader;
  DWORD *pBuf = (DWORD *)pShader->GetBufferPointer();
  fread(pBuf, sizeof(byte), size, fp);
  fclose(fp);

  if (!CParserBin::m_bD3D10)
    hr = D3DXGetShaderConstantTable(pBuf, ppConstantTable);

  assert(hr == S_OK);
#elif defined (DIRECT3D10)
  D3D10CreateBlob(size, (LPD3D10BLOB *)ppShader);
  LPD3D10BLOB pShader = (LPD3D10BLOB)*ppShader;
  DWORD *pBuf = (DWORD *)pShader->GetBufferPointer();
  fread(pBuf, sizeof(byte), size, fp);
  fclose(fp);

  /*{
    ID3D10ShaderReflection *pShaderReflection;
    hr = D3D10ReflectShader(pBuf, size, &pShaderReflection);
    ID3D10Blob* pAsm = NULL;
    D3D10DisassembleShader((UINT *)pBuf, false, NULL, &pAsm);
    if (pAsm)
    {
      const char *szAsm = (char *)pAsm->GetBufferPointer();
      std::vector<SCGBind> InstBindVars;
      char name[256];
      mfPrepareShaderDebugInfo(&m_Insts[m_CurInst], szAsm, name, InstBindVars, (LPD3DXCONSTANTTABLE)pShaderReflection);
    }
    SAFE_RELEASE(pAsm);
    SAFE_RELEASE(pShaderReflection);
  }*/

  //PatchDXBCShaderCode(pShader, this);
  *ppShader = (LPD3DXBUFFER)pShader;
  pBuf = (DWORD *)pShader->GetBufferPointer();
  UINT nSize = pShader->GetBufferSize();

  ID3D10ShaderReflection *pShaderReflection;
  hr = D3D10ReflectShader(pBuf, nSize, &pShaderReflection);
  if (SUCCEEDED(hr))
  {
    *ppConstantTable = (LPD3DXCONSTANTTABLE)pShaderReflection;
  }
  else
  {
    assert(0);
  }
#endif

  remove(szNameSrc);
  remove(szNameDst);

  return true;
}
#endif

ED3DShError CHWShader_D3D::mfFallBack(SHWSInstance *&pInst, int nStatus)
{
  // No fallback for:
  // 1. ShadowGen pass
  // 2. Z-prepass
  // 3. Glow-pass
  // 4. Shadow-pass
#ifdef XENON
  return ED3DShError_CompilingError;
#else
  if (m_eSHClass == eHWSC_Geometry || (gRenDev->m_RP.m_nBatchFilter & (FB_Z | FB_GLOW | FB_SCATTER)) || (gRenDev->m_RP.m_PersFlags & RBPF_SHADOWGEN) || gRenDev->m_RP.m_nPassGroupID == EFSLIST_SHADOW_PASS)
    return ED3DShError_CompilingError;
  if (gRenDev->m_RP.m_pShader)
  {
    if (gRenDev->m_RP.m_pShader->GetShaderType() == eST_HDR || gRenDev->m_RP.m_pShader->GetShaderType() == eST_PostProcess || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Water || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Shadow || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Shadow)
      return ED3DShError_CompilingError;
  }
  // Skip rendering if async compiling Cvar is 2
  if (CRenderer::CV_r_shadersasynccompiling == 2)
    return ED3DShError_CompilingError;

  CShader *pSH = CShaderMan::m_ShaderFallback;
  int nTech = 0;
  if (nStatus == -1)
  {
    pInst->m_Handle.m_bStatus = 1;
    nTech = 1;
  }
  else
  {
    nTech = 0;
    assert(nStatus == 0);
  }
  if (gRenDev->m_RP.m_pShader && gRenDev->m_RP.m_pShader->GetShaderType() == eST_Terrain)
    nTech += 2;

  assert(pSH);
  if (CRenderer::CV_r_logShaders)
  {
    char nameSrc[256];
    mfGetDstFileName(pInst, this, nameSrc, 256, 3);
    gcpRendD3D->LogShv("Async %d: using Fallback tech '%s' instead of 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pSH->m_HWTechniques[nTech]->m_Name.c_str(), pInst, nameSrc);
  }
  // Fallback
  if (pSH)
  {
    if (gRenDev->m_RP.m_CurState & GS_DEPTHFUNC_EQUAL)
    {
      int nState = gRenDev->m_RP.m_CurState & ~GS_DEPTHFUNC_EQUAL;
      nState |= GS_DEPTHWRITE;
      gRenDev->EF_SetState(nState);
    }
    CHWShader_D3D *pHWSH;
    if (m_eSHClass == eHWSC_Vertex)
    {
      pHWSH = (CHWShader_D3D *)pSH->m_HWTechniques[nTech]->m_Passes[0].m_VShader;
#ifdef DO_RENDERLOG
      if (CRenderer::CV_r_log >= 3)
        gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "---- Fallback FX VShader \"%s\"\n", pHWSH->GetName());
#endif
    }
    else
    {
      pHWSH = (CHWShader_D3D *)pSH->m_HWTechniques[nTech]->m_Passes[0].m_PShader;
#ifdef DO_RENDERLOG
      if (CRenderer::CV_r_log >= 3)
        gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "---- Fallback FX PShader \"%s\"\n", pHWSH->GetName());
#endif
    }

    if (!pHWSH->m_Insts.Num())
    {
      int nAsync = CRenderer::CV_r_shadersasynccompiling;
      CRenderer::CV_r_shadersasynccompiling = 0;
      pHWSH->mfPrecache();
      CRenderer::CV_r_shadersasynccompiling = nAsync;
    }
    if (pHWSH->m_Insts.Num())
    {
      pInst = &pHWSH->m_Insts[0];
      m_pCurInst = pInst;
      pInst->m_bFallback = true;
    }
    else
      return ED3DShError_CompilingError;
  }
  //if (nStatus == 0)
  //  return ED3DShError_Compiling;
  return ED3DShError_Ok;
#endif
}

ED3DShError CHWShader_D3D::mfIsValid(SHWSInstance *&pInst, bool bFinalise)
{
  //if (stricmp(m_EntryFunc.c_str(), "FPPS") && stricmp(m_EntryFunc.c_str(), "FPVS") && stricmp(m_EntryFunc.c_str(), "AuxGeomPS") && stricmp(m_EntryFunc.c_str(), "AuxGeomVS"))
  //  return mfFallBack(pInst, 0);

  if (pInst->m_Handle.m_pShader)
    return ED3DShError_Ok;
  if (pInst->m_Handle.m_bStatus == 1)
  {
    return mfFallBack(pInst, -1);
  }
  if (pInst->m_Handle.m_bStatus == 2)
    return ED3DShError_Fake;
  if (pInst->m_Handle.m_pShader == NULL)
  {
#if !defined (XENON) && defined (WIN32)
    if (!bFinalise || !pInst->m_pAsync)
      return ED3DShError_NotCompiled;
    int nStatus = mfAsyncCompileReady(pInst);
    if (nStatus == 1)
    {
      assert(pInst->m_Handle.m_pShader != NULL);
      return ED3DShError_Ok;
    }
    return mfFallBack(pInst, nStatus);
#else
    return ED3DShError_NotCompiled;
#endif
  }
  return ED3DShError_Ok;
}

//========================================================================================

void CHWShader::mfBeginFrame(int nMaxToFlush)
{
#if !defined (XENON) && defined (WIN32)
  //CHWShader_D3D::mfFlushPendedShaders(nMaxToFlush);
#endif
}

SShaderAsyncInfo::~SShaderAsyncInfo()
{
  if (m_pFXShader)
  {
    assert(m_pFXShader->GetID() > 0 && m_pFXShader->GetID() < MAX_REND_SHADERS);
  }
  SAFE_RELEASE(m_pFXShader);
  SAFE_RELEASE(m_pShader);
}


#if defined (WIN32)

void CHWShader::mfFlushPendedShaders()
{
  if (CRenderer::CV_r_shadersasynccompiling > 0)
  {
    iLog->Log("Flushing pended shaders...");
    while (true)
    {
      if (SShaderAsyncInfo::s_nPendingAsyncShaders <= 0)
        break;
    }
    iLog->Log("Finished flushing pended shaders...");
  }
}


int CHWShader_D3D::mfAsyncCompileReady(SHWSInstance *pInst)
{
  //SHWSInstance *pInst = m_pCurInst;
  //assert(pInst->m_pAsync);
	if (!pInst->m_pAsync)
		return 0;

  SShaderAsyncInfo *pAsync = pInst->m_pAsync;
  int nFrame = gRenDev->GetFrameID(false);
  if (pAsync->m_nFrame == nFrame)
  {
    if (pAsync->m_fMinDistance > gRenDev->m_RP.m_fMinDistance)
      pAsync->m_fMinDistance = gRenDev->m_RP.m_fMinDistance;
  }
  else
  {
    pAsync->m_fMinDistance = gRenDev->m_RP.m_fMinDistance;
    pAsync->m_nFrame = nFrame;
  }

  DynArray<SCGBind> InstBindVars;
  ID3DXBuffer* pShader = NULL;
  ID3DXConstantTable *pConstantTable = NULL;
  ID3DXBuffer* pErrorMsgs = NULL;
  string strErr;
  char nmDst[256], nameDst[256], nameSrc[256];
  bool bResult;
  if (pAsync->m_bPending)
    return 0;
  SShaderTechnique *pTech = gRenDev->m_RP.m_pCurTechnique;
  if (!pAsync->m_ProcessInfo.hProcess)
  {
    mfGetDstFileName(pInst, this, nmDst, 256, 3);
    iSystem->GetIPak()->AdjustFileName(nmDst, nameSrc, 0);
    if (pAsync->m_pFXShader && pAsync->m_pFXShader->m_HWTechniques.Num())
      pTech = pAsync->m_pFXShader->m_HWTechniques[0];
    if (pAsync->m_pErrors)
    {
      int nnn = 0;
    }
    if (pAsync->m_pErrors && !pAsync->m_Errors.empty())
    {
      if (CRenderer::CV_r_logShaders)
        gcpRendD3D->LogShv("Async %d: **Failed to compile 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
      string Errors = pAsync->m_Errors;
      string Text = pAsync->m_Text;
      int nRefCount = pAsync->m_pFXShader ? pAsync->m_pFXShader->GetRefCounter() : 0;
      nRefCount = min(nRefCount, pAsync->m_pShader ? pAsync->m_pShader->GetRefCounter() : 0);
      SAFE_DELETE (pInst->m_pAsync);
      if (nRefCount <= 1) // Don't try to compile next combination if shader was deleted
        return -1;
      if (m_Flags & HWSG_AUTOENUMTC)
      {
        if (mfNextProfile(0))
        {
          bool bResult = mfActivate(HWSF_NEXT);
          if (bResult)
            return 1;
          return 0;
        }
      }
      {
        mfOutputCompilerError(Errors, Text.c_str());

				char szGenName[256];
				mfGenName(pInst, szGenName, 256, 1);

        Warning("Couldn't create HW shader '%s%s'", GetName(), szGenName);
        mfSaveCGFile(Text.c_str(), NULL);
      }
      return -1;
    }
    if (CRenderer::CV_r_logShaders)
      gcpRendD3D->LogShv("Async %d: Finished compiling 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
    pShader = pAsync->m_pDevShader;
    pErrorMsgs = pAsync->m_pErrors;
    pConstantTable = pAsync->m_pConstants;
    strErr = pAsync->m_Errors;
    InstBindVars = pAsync->m_InstBindVars;

    if (pAsync->m_bPendedPrint)
      mfPrintCompileInfo(pInst);

    // Load samplers
    if (pAsync->m_bPendedSamplers)
      mfGatherFXParameters(pInst, pInst->m_pBindVars, &InstBindVars, this, 2); 

    if (pAsync->m_bPendedFlush)
    {
      mfFlushCacheFile();
      strcpy(nmDst, GetName());
      char *s = strchr(nmDst, '(');
      if (s)
        s[0] = 0;
      gRenDev->m_cEF.mfInsertNewCombination(m_nMaskGenFX, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, nmDst);
    }

    int nRefCount = pAsync->m_pFXShader ? pAsync->m_pFXShader->GetRefCounter() : 0;
    nRefCount = min(nRefCount, pAsync->m_pShader ? pAsync->m_pShader->GetRefCounter() : 0);
    SAFE_DELETE (pInst->m_pAsync);
    if (nRefCount <= 1) // Just exit if shader was deleted
      return -1;
  }
  else // if the process handle is invalid, the control will bail after WaitForSingleObject fails
  {
		// TODO: The following case picks up results from shader compiling in case r_ShadersIntCompiler = 0 / CV_r_ShadersAsyncCompiling = 1
		// Might have to wait for the output pipe here as well as in case r_ShadersIntCompiler = 0 / CV_r_ShadersAsyncCompiling = 0 to prevent contention
		// See CHWShader_D3D::mfCompileHLSL_Int() for reference
    if (WaitForSingleObject (pAsync->m_ProcessInfo.hProcess, 0) != WAIT_OBJECT_0)
      return 0;

    mfGetDstFileName(pInst, this, nmDst, 256, 2);
    iSystem->GetIPak()->AdjustFileName(nmDst, nameDst, 0);
    mfGetDstFileName(pInst, this, nmDst, 256, 3);
    iSystem->GetIPak()->AdjustFileName(nmDst, nameSrc, 0);
    FILE *fp = iSystem->GetIPak()->FOpen(nameDst, "rb");
    if (!fp)
    {
      if (CRenderer::CV_r_logShaders)
        gcpRendD3D->LogShv("Async %d: **Failed to compile 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
			CloseHandle(pAsync->m_hPipeOutputRead);
			CloseHandle(pAsync->m_hPipeOutputWrite);
      CloseHandle(pAsync->m_ProcessInfo.hProcess);
      CloseHandle(pAsync->m_ProcessInfo.hThread);
      SAFE_DELETE (pInst->m_pAsync);
      remove(nameSrc);
      if (m_Flags & HWSG_AUTOENUMTC)
      {
        if (mfNextProfile(0))
        {
          bool bResult = mfActivate(HWSF_NEXT);
          if (bResult)
            return 1;
          return 0;
        }
      }
      return -1;
    }
    if (CRenderer::CV_r_logShaders)
      gcpRendD3D->LogShv("Async %d: Finished compiling 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
    iSystem->GetIPak()->FClose(fp);
    bResult = mfPostCompiling(nameSrc, nameDst, &pShader, &pConstantTable);
    InstBindVars = pAsync->m_InstBindVars;
    bResult &= mfCreateShaderEnv(pInst, pShader, pConstantTable, pErrorMsgs, InstBindVars, this, true);

		CloseHandle(pInst->m_pAsync->m_hPipeOutputRead);
		CloseHandle(pInst->m_pAsync->m_hPipeOutputWrite);
		CloseHandle(pInst->m_pAsync->m_ProcessInfo.hProcess);
		CloseHandle(pInst->m_pAsync->m_ProcessInfo.hThread);

    int nRefCount = pAsync->m_pShader ? pAsync->m_pShader->GetRefCounter() : 0;
    SAFE_DELETE (pInst->m_pAsync);
    if (nRefCount <= 1) // Just exit if shader was deleted
      return -1;
  }

  if (pErrorMsgs && !strErr.empty())
    return -1;

  bResult = mfUploadHW(pShader);
  SAFE_RELEASE(pShader);

  if (bResult)
  {
    if (pTech)
      mfGetPreprocessFlags(pTech);
    return 1;
  }
  return -1;
}

bool CHWShader_D3D::mfRequestAsync(SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars, const char *prog_text, const char *szProfile, const char *szEntry)
{
  char nameSrc[256], nmDst[256];
  mfGetDstFileName(pInst, this, nmDst, 256, 3);
  iSystem->GetIPak()->AdjustFileName(nmDst, nameSrc, 0);

  if (!m_pCache || !m_pCache->m_pRes[CACHE_USER])
    m_pCache = mfInitCache(NULL, this, true, m_CRC32, false, false);

  pInst->m_pAsync = new SShaderAsyncInfo;
  pInst->m_pAsync->m_fMinDistance = gRenDev->m_RP.m_fMinDistance;
  pInst->m_pAsync->m_nFrame = gRenDev->GetFrameID(false);
  pInst->m_pAsync->m_InstBindVars = InstBindVars;
  pInst->m_pAsync->m_pShader = this;
  pInst->m_pAsync->m_pShader->AddRef();
  pInst->m_pAsync->m_pFXShader = gRenDev->m_RP.m_pShader;
  pInst->m_pAsync->m_pFXShader->AddRef();
  assert(!stricmp(m_NameSourceFX.c_str(), pInst->m_pAsync->m_pFXShader->m_NameFile.c_str()));
  if (m_Flags & HWSG_SHARED)
  {
    assert(m_nMaskGenShader == pInst->m_GLMask);
    TArray<SHWSInstance> *pInstCont = mfGetSharedInstContainer(false, m_nMaskGenShader, false);
    assert(pInstCont);
    pInst->m_pAsync->m_nOwner = pInst - &pInstCont->Get(0);
  }
  else
    pInst->m_pAsync->m_nOwner = pInst - &m_Insts[0];
  pInst->m_pAsync->m_Text = prog_text;
  pInst->m_pAsync->m_Name = szEntry;
  pInst->m_pAsync->m_Profile = szProfile;
  if (CRenderer::CV_r_logShaders)
    gcpRendD3D->LogShv("Async %d: Requested compiling 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
  gcpRendD3D->GetAsyncShaderManager()->InsertPendingShader(pInst->m_pAsync);

  return false;
}
#else
void CHWShader::mfFlushPendedShaders()
{
}
#endif

bool CHWShader_D3D::mfCompileHLSL_Int(char *prog_text, LPD3DXBUFFER* ppShader, LPD3DXCONSTANTTABLE *ppConstantTable, LPD3DXBUFFER* ppErrorMsgs, string& strErr, DynArray<SCGBind>& InstBindVars)
{
  HRESULT hr = S_OK;
  SHWSInstance *pInst = m_pCurInst;
  const char *szProfile = mfProfileToString(pInst->m_eProfileType);
  const char *pFunCCryName = m_EntryFunc.c_str();

#if defined(WIN32) && !defined (OPENGL)

  char nmDst[256], nameDst[512], nameSrc[512];
  mfGetDstFileName(pInst, this, nmDst, 256, 2);
  string nameUser = CRenderer::CV_r_shadersuserfolder ? gRenDev->m_cEF.m_szUserPath + string(nmDst) : string(nmDst);
  iSystem->GetIPak()->AdjustFileName(nameUser.c_str(), nameDst, 0);
  if (!CRenderer::CV_r_shadersintcompiler)
  {
    string path = PathUtil::GetPath(nameDst);
    DWORD dwFileSpecAttr = GetFileAttributes(path);
    if (dwFileSpecAttr == -1)
      iSystem->GetIPak()->MakeDir(path);
    else
      remove(nameDst);
  }
  mfGetDstFileName(pInst, this, nmDst, 256, 3);
  nameUser = CRenderer::CV_r_shadersuserfolder ? gRenDev->m_cEF.m_szUserPath + string(nmDst) : string(nmDst);
  iSystem->GetIPak()->AdjustFileName(nameUser.c_str(), nameSrc, 0);

  if (!CRenderer::CV_r_shadersintcompiler)
  {
    char szCmdLine[1024];
#if defined (DIRECT3D9)
    if (CParserBin::m_bD3D10)
      sprintf(szCmdLine, "fxc.exe /nologo /T %s /Zpr /Gec /Fh \"%s\" \"%s\"", szProfile, nameDst, nameSrc);
    else
      sprintf(szCmdLine, "fxc.exe /nologo /T %s /Zpr /Fo \"%s\" \"%s\"", szProfile, nameDst, nameSrc);
#elif defined (DIRECT3D10)
    sprintf(szCmdLine, "fxc.exe /Gec /nologo /T %s /Zpr /Fo \"%s\" \"%s\"", szProfile, nameDst, nameSrc);
#endif
    strcat(szCmdLine, " /E ");
    strcat(szCmdLine, pFunCCryName);

    // make command for execution
    FILE *fp = fopen(nameSrc, "w");
    if (!fp)
      return NULL;
    fputs(prog_text, fp);
    fclose (fp);

    pInst->m_pAsync = new SShaderAsyncInfo;
    pInst->m_pAsync->m_InstBindVars = InstBindVars;
    pInst->m_pAsync->m_pShader = this;
    pInst->m_pAsync->m_pShader->AddRef();
    pInst->m_pAsync->m_pFXShader = gRenDev->m_RP.m_pShader;
    pInst->m_pAsync->m_pFXShader->AddRef();
    if (m_Flags & HWSG_SHARED)
    {
      assert(m_nMaskGenShader == pInst->m_GLMask);
      TArray<SHWSInstance> *pInstCont = mfGetSharedInstContainer(false, m_nMaskGenShader, false);
      assert(pInstCont);
      pInst->m_pAsync->m_nOwner = pInst - &pInstCont->Get(0);
    }
    else
      pInst->m_pAsync->m_nOwner = pInst - &m_Insts[0];

		// create pipe
		SECURITY_ATTRIBUTES sa;
		ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = 0;

		if (!CreatePipe(&pInst->m_pAsync->m_hPipeOutputRead, &pInst->m_pAsync->m_hPipeOutputWrite, &sa, 0))
		{
			SAFE_DELETE (pInst->m_pAsync);
			iLog->LogError("CreatePipe failed! Needed for out of process shader compilation.");
			return false;
		}

		// create process
		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.hStdOutput = pInst->m_pAsync->m_hPipeOutputWrite;
		si.hStdError   = pInst->m_pAsync->m_hPipeOutputWrite;
		si.dwFlags |= STARTF_USESTDHANDLES;

		ZeroMemory(&pInst->m_pAsync->m_ProcessInfo, sizeof(PROCESS_INFORMATION));

		if( !CreateProcess( NULL, // No module name (use command line). 
			szCmdLine,        // Command line. 
			NULL,             // Process handle not inheritable. 
			NULL,             // Thread handle not inheritable. 
			TRUE,             // Set handle inheritance to TRUE. 
			CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, // No creation flags. 
			NULL,             // Use parent's environment block. 
			NULL,             // Set starting directory. 
			&si,              // Pointer to STARTUPINFO structure.
			&pInst->m_pAsync->m_ProcessInfo )             // Pointer to PROCESS_INFORMATION structure.
			) 
		{
			CloseHandle(pInst->m_pAsync->m_hPipeOutputRead);
			CloseHandle(pInst->m_pAsync->m_hPipeOutputWrite);
			SAFE_DELETE (pInst->m_pAsync);
			iLog->LogError("CreateProcess failed: %s", szCmdLine);
			return false;
		}

		if (CRenderer::CV_r_shadersasynccompiling && !(m_Flags & HWSG_SYNC))
		{
			if (CRenderer::CV_r_logShaders)
				gcpRendD3D->LogShv("Async %d: Requested compiling 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pInst, nameSrc);
			return false;
		}

		DWORD waitResult = 0;
		HANDLE waitHandles[] = { pInst->m_pAsync->m_ProcessInfo.hProcess, pInst->m_pAsync->m_hPipeOutputRead };
		while(true)
		{
			waitResult = WaitForMultipleObjects(sizeof(waitHandles) / sizeof(waitHandles[0]), waitHandles, FALSE, 60000L);
			if (waitResult == WAIT_FAILED)
				break;

			DWORD bytesRead, bytesAvailable;
			while(PeekNamedPipe(pInst->m_pAsync->m_hPipeOutputRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable)
			{
				char buff[4096];
				ReadFile(pInst->m_pAsync->m_hPipeOutputRead, buff, sizeof(buff)-1, &bytesRead, 0);
				buff[bytesRead] = '\0';
				strErr += buff;
			}

			if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_TIMEOUT)
				break;
		}

		CloseHandle(pInst->m_pAsync->m_hPipeOutputRead);
		CloseHandle(pInst->m_pAsync->m_hPipeOutputWrite);

		if (waitResult == WAIT_TIMEOUT)
		{
			iLog->LogWarning ("fxc takes forever to compile shader. Timeout of 60 seconds reached... terminating process!");
			TerminateProcess(pInst->m_pAsync->m_ProcessInfo.hProcess, 0);
		}

		bool bResult = mfPostCompiling(nameSrc, nameDst, ppShader, ppConstantTable);

    if (CRenderer::CV_r_logShaders > 1)
    {
      if (bResult)
        gcpRendD3D->LogShv("Sync: Succeeded compiling '%s' shader\n", nameSrc);
      else
        gcpRendD3D->LogShv("Sync: Failed compiling '%s' shader\n", nameSrc);
    }

    CloseHandle(pInst->m_pAsync->m_ProcessInfo.hProcess);
    CloseHandle(pInst->m_pAsync->m_ProcessInfo.hThread);

    SAFE_DELETE (pInst->m_pAsync);

    if (CRenderer::CV_r_shadersdebug == 2)
    {
#if defined(PS3)
      mfSaveCGFile(prog_text, "Shaders/Cache/D3D10/testcg");
#else
      mfSaveCGFile(prog_text, "TestCG");
#endif
    }

    return bResult;
  }
  else
#endif // WIN32
  {
    bool bRes = true; 
#if defined (DIRECT3D9) || defined(OPENGL)
    int nFlags = D3DXSHADER_PACKMATRIX_ROWMAJOR; // | D3DXSHADER_AVOID_FLOW_CONTROL;// |  D3DXSHADER_USE_LEGACY_D3DX9_31_DLL;
  #ifdef XENON
    nFlags |= D3DXSHADER_MICROCODE_BACKEND_NEW;
  #endif
    if (CRenderer::CV_r_shadersdebug == 2)
    {
#if defined(PS3)
      mfSaveCGFile(prog_text, "Shaders/Cache/D3D10/testcg");
#else
      mfSaveCGFile(prog_text, "TestCG");
#endif
    }
  #if !defined (XENON) && defined (WIN32)
    if (CRenderer::CV_r_shadersasynccompiling && !(m_Flags & HWSG_SYNC))
    {
      return mfRequestAsync(pInst, InstBindVars, prog_text, szProfile, pFunCCryName);
    }
    else
  #endif
    {
      hr = D3DXCompileShader(prog_text, strlen(prog_text), NULL, NULL, pFunCCryName, szProfile, nFlags, ppShader, ppErrorMsgs, ppConstantTable); 
      if (FAILED(hr))
      {
        if (*ppErrorMsgs)
        {
          const char *err = (const char *)ppErrorMsgs[0]->GetBufferPointer();
          strErr += err;
        }
        else
        { 
          strErr += "D3DXCompileShader failed";
        }
        bRes = false;
      }
      return bRes;
    }
#elif defined (DIRECT3D10)
#if !defined (XENON) && defined (WIN32)
    if (CRenderer::CV_r_shadersdebug == 2)
    {
#if defined(PS3)
      mfSaveCGFile(prog_text, "Shaders/Cache/D3D10/testcg");
#else
      mfSaveCGFile(prog_text, "TestCG");
#endif
    }
    if (CRenderer::CV_r_shadersasynccompiling && !(m_Flags & HWSG_SYNC))
    {
      return mfRequestAsync(pInst, InstBindVars, prog_text, szProfile, pFunCCryName);
    }
    else
#endif
    {
      hr = D3DX10CompileFromMemory(prog_text,
        strlen(prog_text),
        GetName(),
        NULL,
        NULL,
        pFunCCryName,
        szProfile,
        D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY,
        0,
        NULL,
        (ID3D10Blob **)ppShader,
        (ID3D10Blob **)ppErrorMsgs, &hr);
      if (FAILED(hr) || !*ppShader)
      {
        if (*ppErrorMsgs)
        {
          const char *err = (const char *)ppErrorMsgs[0]->GetBufferPointer();
          strErr += err;
        }
        else
        {
          strErr += "D3DXCompileShader failed";
        }
        bRes = false;
      }
      else
      {
        ID3D10ShaderReflection *pShaderReflection;
        UINT *pData = (UINT *)ppShader[0]->GetBufferPointer();
        UINT nSize = ppShader[0]->GetBufferSize();
        hr = D3D10ReflectShader(pData, nSize, &pShaderReflection);
        if (SUCCEEDED(hr))
        {
          *ppConstantTable = (LPD3DXCONSTANTTABLE)pShaderReflection;
        }
        else
        {
          assert(0);
        }
      }
      return bRes;
    }
#endif
  }
  return false;
}

bool CHWShader_D3D::mfNextProfile(uint nFlags)
{
  CD3D9Renderer *rd = gcpRendD3D;
  SHWSInstance *pInst = m_pCurInst;
  if (!(rd->GetFeatures() & RFT_HW_PS20))
    return false;
  switch (pInst->m_eProfileType)
  {
  case eHWSP_PS_2_X:
  case eHWSP_VS_2_0:
  case eHWSP_PS_3_0:
  case eHWSP_PS_4_0:
  case eHWSP_GS_4_0:
  case eHWSP_VS_4_0:
  case eHWSP_VS_3_0:
    return false;
  case eHWSP_VS_1_1:
    pInst->m_eProfileType = eHWSP_VS_2_0;
    break;

  case eHWSP_PS_1_1:
    pInst->m_eProfileType = eHWSP_PS_2_0;
    break;
  case eHWSP_PS_2_0:
    if (rd->m_Features & RFT_HW_PS2X)
      pInst->m_eProfileType = eHWSP_PS_2_X;
    else
      return false;
    break;
  }

  return true;
}
LPD3DXBUFFER CHWShader_D3D::mfCompileHLSL(char *prog_text, LPD3DXCONSTANTTABLE *ppConstantTable, LPD3DXBUFFER* ppErrorMsgs, uint nFlags, DynArray<SCGBind>& InstBindVars)
{
  // Test adding source text to context
  SHWSInstance *pInst = m_pCurInst;
  string strErr;
  LPD3DXBUFFER pCode = NULL;
  HRESULT hr = S_OK;
  bool bResult = mfCompileHLSL_Int(prog_text, &pCode, ppConstantTable, ppErrorMsgs, strErr, InstBindVars);
  if (!pCode)
  {
    if (CRenderer::CV_r_shadersasynccompiling)
      return NULL;
    if (m_Flags & HWSG_AUTOENUMTC)
    {
      while (mfNextProfile(nFlags))
      {
        bResult = mfCompileHLSL_Int(prog_text, &pCode, ppConstantTable, ppErrorMsgs, strErr, InstBindVars);
        if (pCode)
          break;
        if (pInst->IsAsyncCompiling())
          return NULL;
      }
    }
    if (!pCode)
    {
      //int nLights = pInst->m_LightMask & 0xf;
      //if (nLights > 1)
      //  iLog->Log("WARNING: Shader '%s' was failed to compile for %d light sources (fallback to %d light sources)", GetName(), nLights, nLights-1);
      //else
      {
        mfOutputCompilerError(strErr, prog_text);

				char szGenName[256];
				mfGenName(pInst, szGenName, 256, 1);

        Warning("Couldn't create HW shader '%s%s'", GetName(), szGenName);
        mfSaveCGFile(prog_text, NULL);
      }
    }
  }

  return pCode;
}

TArray<CHWShader_D3D::SHWSInstance> *CHWShader_D3D::mfGetSharedInstContainer(bool bCreate, uint GLMask, bool bPrecache)
{
  TArray<SHWSInstance> *pInstCont;
  InstanceMapItor itInst = m_SharedInsts.find(m_EntryFunc);
  SHWSSharedList *pInstSH = NULL;
  if (itInst == m_SharedInsts.end())
  {
    if (!bCreate)
      return NULL;
    pInstSH = new SHWSSharedList;
    m_SharedInsts.insert(InstanceMapItor::value_type(m_EntryFunc, pInstSH));
  }
  else
    pInstSH = itInst->second;
  int i;
  SHWSSharedInstance *pSHI;
  for (i=0; i<pInstSH->m_SharedInsts.Num(); i++)
  {
    pSHI = &pInstSH->m_SharedInsts[i];
    if (pSHI->m_GLMask == GLMask)
      break;
  }
  if (i == pInstSH->m_SharedInsts.Num())
  {
    if (!bCreate)
      return NULL;
    pSHI = pInstSH->m_SharedInsts.AddIndex(1);
    memset(pSHI, 0, sizeof(SHWSSharedInstance));
    pSHI->m_GLMask = GLMask;
  }
  if (bPrecache)
  {
    const char *nm = gRenDev->m_RP.m_pShader->m_NameShader.c_str();
    for (i=0; i<pInstSH->m_SharedNames.size(); i++)
    {
      SHWSSharedName *pSHN = &pInstSH->m_SharedNames[i];
      if (!stricmp(pSHN->m_Name.c_str(), nm))
        break;
    }
    if (i == pInstSH->m_SharedNames.size())
    {
      SHWSSharedName NM;
      NM.m_Name = nm;
      NM.m_CRC32 = m_CRC32;
      pInstSH->m_SharedNames.push_back(NM);
    }
  }
  pInstCont = &pSHI->m_Insts;

  return pInstCont;
}

void CHWShader_D3D::mfPrepareShaderDebugInfo(SHWSInstance *pInst, CHWShader_D3D *pSH, const char *szAsm, DynArray<SCGBind>& InstBindVars, LPD3DXCONSTANTTABLE pConstantTable)
{
  if (szAsm)
  {
    char *szInst = strstr((char *)szAsm, "pproximately ");
    if (szInst)
      pInst->m_nInstructions = atoi(&szInst[13]);
  }
  if (CRenderer::CV_r_shadersdebug)
  {
    char nmdst[256];
    mfGetDstFileName(pInst, pSH, nmdst, 256, 1);
#ifdef XENON
    char *s = strstr(nmdst, "(RT");
    if (s)
      s[0] = '/';
    s = strstr(nmdst, "(GL");
    if (s)
      s[0] = '/';
#endif
    string szName = CRenderer::CV_r_shadersuserfolder ? gRenDev->m_cEF.m_szUserPath + string(nmdst) + string(".fxca") : string(nmdst) + string(".fxca");

    FILE *statusdst = iSystem->GetIPak()->FOpen(szName.c_str(), "wb");
    if (statusdst)
    {
      iSystem->GetIPak()->FPrintf(statusdst, "\n// %s %s\n\n", "%STARTSHADER", mfProfileToString(pInst->m_eProfileType));
      if (pSH->m_eSHClass == eHWSC_Vertex)
      {
        for (uint i=0; i<InstBindVars.size(); i++)
        {
          SCGBind *pBind = &InstBindVars[i];
          iSystem->GetIPak()->FPrintf(statusdst, "//   %s %s %d %d\n", "%%", pBind->m_Name.c_str(), pBind->m_nParameters, pBind->m_dwBind);
        }
      }
      iSystem->GetIPak()->FPrintf(statusdst, "%s", szAsm);
      iSystem->GetIPak()->FPrintf(statusdst, "\n// %s\n", "%ENDSHADER");
      iSystem->GetIPak()->FClose(statusdst);
    }
    pInst->m_Handle.m_pShader = NULL;
  }
}

void CHWShader_D3D::mfPrintCompileInfo(SHWSInstance *pInst)
{
  int nConsts = 0;
  int nParams = pInst->m_pBindVars ? pInst->m_pBindVars->size() : 0;
  for (int i=0; i<nParams; i++)
  {
    SCGBind *pB = &(*pInst->m_pBindVars)[i];
    nConsts += pB->m_nParameters;
  }

  char szGenName[512];
  strncpy(szGenName, GetName(), 512);
  char *s = strchr(szGenName, '(');
  if (s)
    s[0] = 0;
  if (CRenderer::CV_r_shadersdebug == 2)
  {
    const char *pName = gRenDev->m_cEF.mfInsertNewCombination(m_nMaskGenFX, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, szGenName, false);
    CryLog(" Compile %s (%d instructions, %d/%d constants) ... ", pName, pInst->m_nInstructions, nParams, nConsts);
    int nSize = strlen(szGenName);
    mfGenName(pInst, &szGenName[nSize], 512, 1);
    CryLog("           --- Cache entry: %s", szGenName);
  }
  else
  {
    int nSize = strlen(szGenName);
    mfGenName(pInst, &szGenName[nSize], 512, 1);
    CryLog(" Compile %s (%d instructions, %d/%d constants) ... ", szGenName, pInst->m_nInstructions, nParams, nConsts);
  }

  if (gRenDev->m_cEF.m_bActivated)
    CryLog(
    " Shader %s"
#if defined(__GNUC__)
    "(%llx)"
#else
    "(%I64x)"
#endif
    "(%x)(%x)(%x)(%s) wasn't compiled before preactivating phase",
    GetName(), pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask,  mfProfileToString(pInst->m_eProfileType));
}
class CSpinLock
{
public:
	CSpinLock()
	{
#if defined (WIN32) || defined(XENON)
		while ( InterlockedCompareExchange(&s_locked, 1L, 0L) == 1L )
			Sleep(0);
#endif
	}

	~CSpinLock()
	{
#if defined (WIN32) || defined(XENON)
		InterlockedExchange(&s_locked, 0L);
#endif
	}

private:
	static volatile LONG s_locked;
};

volatile LONG CSpinLock::s_locked = 0L;

bool CHWShader_D3D::mfCreateShaderEnv(SHWSInstance *pInst, LPD3DXBUFFER pShader, LPD3DXCONSTANTTABLE pConstantTable, LPD3DXBUFFER pErrorMsgs, DynArray<SCGBind>& InstBindVars, CHWShader_D3D *pSH, bool bShaderThread)
{
  // Create asm (.fxca) cache file
  assert(pInst);
  if (!pInst)
    return false;

  CSpinLock lock;

  if (pInst->m_pBindVars)
    return true;

  if (pShader && (gRenDev->m_cEF.m_nCombinations < 0))
  {
#if defined(DIRECT3D9) || defined(OPENGL)
    if (!CParserBin::m_bD3D10)
    {
      LPD3DXBUFFER pAsm = NULL;
      D3DXDisassembleShader((DWORD *)pShader->GetBufferPointer(), FALSE, NULL, &pAsm);
      if (pAsm)
      {
        char *szAsm = (char *)pAsm->GetBufferPointer();
        mfPrepareShaderDebugInfo(pInst, pSH, szAsm, InstBindVars, pConstantTable);
      }
      SAFE_RELEASE(pAsm);
    }
#elif defined(DIRECT3D10)
    ID3D10Blob* pAsm = NULL;
    ID3D10Blob* pSrc = (ID3D10Blob *)pShader;
    UINT *pBuf = (UINT *)pSrc->GetBufferPointer();
    D3D10DisassembleShader(pBuf, pSrc->GetBufferSize(), false, NULL, &pAsm);
    if (pAsm)
    {
      char *szAsm = (char *)pAsm->GetBufferPointer();
      mfPrepareShaderDebugInfo(pInst, pSH, szAsm, InstBindVars, pConstantTable);
    }
    SAFE_RELEASE(pAsm);
#endif
  }
  //assert(!pInst->m_pBindVars);

  if (pShader)
  {
    if (pSH->m_eSHClass == eHWSC_Vertex)
      mfVertexFormat(pInst, pSH, pShader, InstBindVars);
    {
#ifdef OPENGL
      if (pInst->m_Handle.IsValid() == ED3DShError_Ok)
      {
        SAFE_RELEASE(pConstantTable);
        D3DXBuildConstantsTable((IDirect3DBaseShader9*)pInst->m_Handle.m_pHandle, &pConstantTable);
      }
#endif
    }
  }
  if (pConstantTable)
    mfCreateBinds(pInst, pConstantTable, (byte *)pShader->GetBufferPointer(), pShader->GetBufferSize());
  if (!(pSH->m_Flags & HWSG_PRECACHEPHASE))
  {
    int nConsts = 0;
    int nParams = pInst->m_pBindVars ? pInst->m_pBindVars->size() : 0;
    for (int i=0; i<nParams; i++)
    {
      SCGBind *pB = &(*pInst->m_pBindVars)[i];
      nConsts += pB->m_nParameters;
    }
    if (gRenDev->m_cEF.m_nCombinations > 0)
    {
      assert(!bShaderThread);

			char szGenName[256];
			mfGenName(pInst, szGenName, 256, 1);

      //if (!(gRenDev->m_cEF.m_nCombination & 0xff))              
      if (!CParserBin::m_bD3D10)
      {
        CryLog(" Compile %s %s%s (%d out of %d) - (%d instructions, %d/%d constants) ... ", 
          mfProfileToString(pInst->m_eProfileType), pSH->GetName(), szGenName, gRenDev->m_cEF.m_nCombination, gRenDev->m_cEF.m_nCombinations, 
          pInst->m_nInstructions,nParams,nConsts );
      }
      else
      {
        CryLog(" Compile %s %s%s (%d out of %d) ... ", 
          mfProfileToString(pInst->m_eProfileType), pSH->GetName(), szGenName, gRenDev->m_cEF.m_nCombination, gRenDev->m_cEF.m_nCombinations);
      }
      //else
      {
        //iLog->LogToConsole(" Compile %s %s (%d out of %d)- %d instructions ... ", mfProfileToString(pInst->m_eProfileType), GetName(), gRenDev->m_cEF.m_nCombination, gRenDev->m_cEF.m_nCombinations, pInst->m_nInstructions);
      }
    }
    else
    {
      if (!bShaderThread)
        pSH->mfPrintCompileInfo(pInst);
#if !defined(XENON) && !defined(PS3)
      else
      if (pInst->m_pAsync)
        pInst->m_pAsync->m_bPendedPrint = true;
#endif
    }
  }
#if defined(DIRECT3D9) || defined(OPENGL)
  assert(!pInst->m_pBindVars || pInst->m_pBindVars->size()<=30);
#endif

  mfGatherFXParameters(pInst, pInst->m_pBindVars, &InstBindVars, pSH, bShaderThread ? 1 : 0); 

  if (pShader)
    mfCreateCacheItem(pInst, InstBindVars, (byte *)pShader->GetBufferPointer(), pShader->GetBufferSize(), -1, pSH, bShaderThread);
#if !defined (XENON)
  else
    mfCreateCacheItem(pInst, InstBindVars, NULL, 0, -1, pSH, bShaderThread);
#endif

#if defined(PS3) && !defined(CRY_USE_GCM)
  if (pShader != NULL)
    ClearShaderProgram((CGprogram)pShader->GetBufferPointer());
#endif
#if defined(DIRECT3D9) || defined(OPENGL)
  SAFE_RELEASE(pConstantTable);
  SAFE_RELEASE(pErrorMsgs);
#elif defined (DIRECT3D10)
  ID3D10ShaderReflection *pRFL = (ID3D10ShaderReflection *)pConstantTable;
  ID3D10Blob *pER = (ID3D10Blob *)pErrorMsgs;
  SAFE_RELEASE(pRFL);
  SAFE_RELEASE(pER);
#endif

  return true;
}

// Compile pixel/vertex shader for the current instance properties
bool CHWShader_D3D::mfActivate(uint nFlags)
{
  PROFILE_FRAME(Shader_HWShaderActivate);

  bool bResult = true;
  SHWSInstance *pInst = m_pCurInst;
  if (mfIsValid(pInst, true) == ED3DShError_NotCompiled)
  {
    if (!(m_Flags & HWSG_PRECACHEPHASE) && !(nFlags & HWSF_NEXT))
      mfSetHWStartProfile(nFlags);

    bool bCreate = false;

    char namedst[256];
    char nameCache[256];
    mfGetDstFileName(pInst, this, namedst, 256, 0);
    fpStripExtension(namedst, nameCache);
    fpAddExtension(nameCache, ".fxcb");

    if (!m_pCache || m_pCache->m_bD3D10 != CParserBin::m_bD3D10)
    {
      SAFE_RELEASE(m_pCache);
      m_pCache = mfInitCache(nameCache, this, true, m_CRC32, ((gRenDev->m_cEF.m_nCombinations > 0) || !CRenderer::CV_r_shadersuserfolder), true);
    }
    if (gRenDev->m_cEF.m_nCombinations > 0)
    {
      FXShaderCacheNamesItor it = m_ShaderCacheList.find(nameCache);
      if (it == m_ShaderCacheList.end())
        m_ShaderCacheList.insert(FXShaderCacheNamesItor::value_type(nameCache, m_CRC32));
    }
    uint nSize;
    SShaderCacheHeaderItem *pCacheItem = mfGetCacheItem(nFlags, nSize);
    if (pCacheItem && pCacheItem->m_Profile != 255)
    {
      //assert (!(m_Flags & HWSG_PRECACHEPHASE));
      // Duplicated item
      if (m_Flags & HWSG_PRECACHEPHASE)
        return true;
      bool bRes = false;
      bRes = mfActivateCacheItem(pCacheItem, nSize);
      mfFreeCacheItem(pInst, m_pCache);
      if (!(m_Flags & HWSG_PRECACHEPHASE))
      {
        if (gRenDev->m_cEF.m_nCombinations < 0 && gRenDev->m_cEF.m_bActivated)
        {
          iLog->LogToConsole(
            " Shader %s"
#if defined(__GNUC__)
            "(%llx)"
#else
            "(%I64x)"
#endif
            "(%x)(%x)(%x)(%s) wasn't activated during preactivating phase",
            GetName(), pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask,  mfProfileToString(pInst->m_eProfileType));


          char name[128];
          strcpy(name, GetName());
          char *s = strchr(name, '(');
          if (s)
            s[0] = 0;
          const char *str = gRenDev->m_cEF.mfInsertNewCombination(m_nMaskGenFX, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, name, false);
          iLog->LogToFile(" --Shader '%s' wasn't activated during preactivating phase", str);
        }
      }
      if (bRes)
        return (pInst->m_Handle.m_pShader != NULL);
      pCacheItem = NULL;
    }
    else
    if (pCacheItem && pCacheItem->m_Profile == 255)
    {
      return false;
    }
    else
    if (gRenDev->m_cEF.m_bActivatePhase)
    {
      iLog->Log(
        "Warning: Shader %s"
#if defined(__GNUC__)
        "(%llx)"
#else
        "(%I64x)"
#endif
        "(%x)(%x)(%x)(%s) wasn't compiled before preactivating phase",
        GetName(), pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask,  mfProfileToString(pInst->m_eProfileType));
      return false;
    }
#if defined PS3
		//if shader compilation is forced to be disabled
		if(gPS3Env->bDisableCgc) 
			return false;
#endif
    fTime0 = iTimer->GetAsyncCurTime();
    LPD3DXBUFFER pShader = NULL;
    LPD3DXCONSTANTTABLE pConstantTable = NULL;
    LPD3DXBUFFER pErrorMsgs = NULL;
    DynArray<SCGBind> InstBindVars;
    m_Flags |= HWSG_WASGENERATED;
    char *scr = mfGenerateScript(pInst, InstBindVars, nFlags);

    {
      PROFILE_FRAME(Shader_CompileHLSL);

#if defined(PS3) && !defined(CRY_USE_GCM)
      // Pass the shader name to the shader compiler (for debugging).
      {
        char nameSuffix[256];
        mfGenName(nameSuffix, sizeof nameSuffix - 1, true);
        nameSuffix[sizeof nameSuffix - 1] = 0;
        snprintf(ps3ShaderName, ps3ShaderName_size - 1,
          "%s%s", GetName(), nameSuffix);
        ps3ShaderName[ps3ShaderName_size - 1] = 0;
      }
#endif
      pShader = mfCompileHLSL(scr, &pConstantTable, &pErrorMsgs, nFlags, InstBindVars);

#if defined(PS3) && !defined(CRY_USE_GCM)
      ps3ShaderName[0] = 0;
#endif
    }
    if (!pShader && pInst->IsAsyncCompiling())
      return false;

    bResult = mfCreateShaderEnv(pInst, pShader, pConstantTable, pErrorMsgs, InstBindVars, this, false);
    bResult &= mfUploadHW(pShader);
    SAFE_RELEASE(pShader);

    fTime0 = iTimer->GetAsyncCurTime() - fTime0;
    //iLog->LogToConsole(" Time activate: %.3f", fTime0);
  }
  bool bSuccess = (mfIsValid(pInst, true) == ED3DShError_Ok);

  return bSuccess;
}

CHWShader_D3D::SHWSInstance *CHWShader_D3D::mfGetInstance(int nInstance, uint GLMask)
{
  TArray<SHWSInstance> *pInstCont = &m_Insts;
  if (m_Flags & HWSG_SHARED)
  {
    pInstCont = mfGetSharedInstContainer(true, GLMask, false);
    assert(pInstCont);
  }
  return &pInstCont->Get(nInstance);
}

static bool IsD3D10Profile(EHWSProfile ePr)
{
  if (ePr==eHWSP_PS_4_0 || ePr==eHWSP_VS_4_0 || ePr==eHWSP_GS_4_0)
    return true;
  return false;
}
CHWShader_D3D::SHWSInstance *CHWShader_D3D::mfGetInstance(uint64 RTMask, uint LightMask, uint GLMask, uint MDMask, uint MDVMask, uint nFlags)
{
  SHWSInstance *cgi;
  if ((m_Flags & HWSG_SHARED) && m_nInstFrame != m_nCurInstFrame)
  {
    m_nCurInstFrame = m_nInstFrame;
    m_pCurInst = NULL;
  }
  if (m_pCurInst)
  {
    cgi = m_pCurInst;
    if (cgi->m_RTMask == RTMask && cgi->m_GLMask == GLMask && cgi->m_LightMask == LightMask && cgi->m_MDMask == MDMask && cgi->m_MDVMask == MDVMask)
    {
#if !defined(PS3)
      if (CParserBin::m_bD3D10 == IsD3D10Profile(cgi->m_eProfileType))
#endif
        return cgi;
    }
  } 
  uint i;
  TArray<SHWSInstance> *pInstCont = &m_Insts;
  if (m_Flags & HWSG_SHARED)
    pInstCont = mfGetSharedInstContainer(true, GLMask, (nFlags & HWSF_PRECACHE_INST)!=0);
  for (i=0; i<pInstCont->Num(); i++)
  {
    cgi = &pInstCont->Get(i);
    if (cgi->m_RTMask == RTMask && cgi->m_GLMask == GLMask && cgi->m_LightMask == LightMask && cgi->m_MDMask == MDMask && cgi->m_MDVMask == MDVMask)
    {
#if !defined(PS3)
      if (CParserBin::m_bD3D10 == IsD3D10Profile(cgi->m_eProfileType))
#endif
      {
        m_pCurInst = cgi;
        return cgi;
      }
    }
  }
  cgi = pInstCont->AddIndex(1);
  memset(cgi, 0, sizeof(*cgi));
  cgi->m_nVertexFormat = 1;
  cgi->m_nCache = -1;
  m_nInstFrame++;

  cgi->m_RTMask = RTMask;
  cgi->m_GLMask = GLMask;
  cgi->m_LightMask = LightMask;
  cgi->m_MDMask = MDMask;
  cgi->m_MDVMask = MDVMask;
  m_pCurInst = cgi;
  return cgi;
}


bool CHWShader_D3D::mfSet(int nFlags)
{
  PROFILE_FRAME(Shader_SetShaders);

  CD3D9Renderer *rd = gcpRendD3D;

  uint LTMask = rd->m_RP.m_FlagsShader_LT;
  uint64 RTMask = rd->m_RP.m_FlagsShader_RT & m_nMaskAffect_RT;
  uint MDMask = rd->m_RP.m_FlagsShader_MD;
  uint MDVMask = rd->m_RP.m_FlagsShader_MDV;

  if (LTMask)
  {
    if (!(m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING | HWSG_FP_EMULATION)))
      LTMask = 0;
    else
    if (!(m_Flags & HWSG_SUPPORTS_MULTILIGHTS) && (m_Flags & HWSG_SUPPORTS_LIGHTING))
    {
      int nLightType = (LTMask >> SLMF_LTYPE_SHIFT) & SLMF_TYPE_MASK;
      if (nLightType != SLMF_PROJECTED)
        LTMask = 1;
    }
  }

  if (m_eSHClass == eHWSC_Pixel)
  {
    MDVMask = 0;
    MDMask &= ~HWMD_TCMASK;
  }

  SHWSInstance *pInst = mfGetInstance(RTMask, LTMask, m_nMaskGenShader, MDMask, MDVMask, nFlags);

  // Update texture modificator flags based on active samplers state
  if (m_eSHClass == eHWSC_Pixel && (nFlags & HWSF_SETTEXTURES))
  {
    int nResult = mfCheckActivation(pInst, nFlags);
    if (!nResult)
    {
#ifdef DIRECT3D10
      CHWShader_D3D::m_pCurInstPS = NULL;
#endif
      return false;
    }
    mfUpdateSamplers();
    
		uint64 NormalmapMask = g_HWSR_MaskBit[HWSR_NORMAL_DXT5];

    if (((rd->m_RP.m_FlagsShader_MD ^ MDMask) & ~HWMD_TCMASK) || ((RTMask ^ rd->m_RP.m_FlagsShader_RT) & NormalmapMask))
    {
      MDMask = rd->m_RP.m_FlagsShader_MD & ~HWMD_TCMASK;
      RTMask = (RTMask | (rd->m_RP.m_FlagsShader_RT & NormalmapMask)) & m_nMaskAffect_RT;
      pInst = mfGetInstance(RTMask, LTMask, m_nMaskGenShader, MDMask, MDVMask, nFlags);
    }
  }
  if (CRenderer::CV_r_measureoverdraw>0 && CRenderer::CV_r_measureoverdraw<4)
  {
    if (mfIsValid(pInst, false) ==  ED3DShError_NotCompiled)
      mfActivate(nFlags);
    RTMask |= g_HWSR_MaskBit[HWSR_DEBUG0] | g_HWSR_MaskBit[HWSR_DEBUG1] | g_HWSR_MaskBit[HWSR_DEBUG2] | g_HWSR_MaskBit[HWSR_DEBUG3];
    RTMask &= m_nMaskAffect_RT;
    if (CRenderer::CV_r_measureoverdraw == 1 && m_eSHClass == eHWSC_Pixel)
      rd->m_RP.m_NumShaderInstructions = pInst->m_nInstructions;
    else
    if (CRenderer::CV_r_measureoverdraw == 3 && m_eSHClass == eHWSC_Vertex)
      rd->m_RP.m_NumShaderInstructions = pInst->m_nInstructions;
    else
    if (CRenderer::CV_r_measureoverdraw == 2)
      rd->m_RP.m_NumShaderInstructions = 30;

    pInst = mfGetInstance(RTMask, LTMask, m_nMaskGenShader, MDMask, MDVMask, nFlags);
  }

  if (!mfCheckActivation(pInst, nFlags))
  {
#ifdef DIRECT3D10
    if (m_eSHClass == eHWSC_Vertex)
      m_pCurInstVS = NULL;
    else
    if (m_eSHClass == eHWSC_Pixel)
      m_pCurInstPS = NULL;
#endif
    return false;
  }

#ifdef DO_RENDERLOG
  if (CRenderer::CV_r_log >= 3)
  {
    if (m_eSHClass == eHWSC_Pixel)
    {
#if defined(__GNUC__)
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX PShader \"%s\" (%d instr)LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#else
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX PShader \"%s\" (%d instr) LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#endif
    }
    else
    if (m_eSHClass == eHWSC_Vertex)
    {
#if defined(__GNUC__)
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX VShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#else
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX VShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#endif
    }
    else
    if (m_eSHClass == eHWSC_Geometry)
    {
#if defined(__GNUC__)
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX GShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#else
      rd->Logv(SRendItem::m_RecurseLevel, "--- Set FX GShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, pInst->m_LightMask, m_nMaskGenShader, RTMask, MDMask, MDVMask);
#endif
    }
  }
#endif

  if (m_eSHClass == eHWSC_Pixel)
  {
    if (m_nFrame != rd->m_nFrameUpdateID)
    {
      m_nFrame = rd->m_nFrameUpdateID;
      rd->m_RP.m_PS.m_NumPShaders++;
      if (pInst->m_nInstructions > rd->m_RP.m_PS.m_NumPSInstructions)
      {
        rd->m_RP.m_PS.m_NumPSInstructions = pInst->m_nInstructions;
        rd->m_RP.m_PS.m_pMaxPShader = this;
        rd->m_RP.m_PS.m_pMaxPSInstance = pInst;
      }
    }
    if (!(nFlags & HWSF_PRECACHE))
    {
      if (m_pCurPS != pInst->m_Handle.m_pShader)
      {
        m_pCurPS = pInst->m_Handle.m_pShader;
        rd->m_RP.m_PS.m_NumPShadChanges++;
        mfBind();
      }
#ifdef DIRECT3D10
      m_pCurInstPS = pInst;
#endif
      mfSetParameters(0);
      if (nFlags & HWSF_SETTEXTURES)
        mfSetSamplers();
    }
  }
  else
  if (m_eSHClass == eHWSC_Vertex)
  {
    if (m_nFrame != rd->m_nFrameUpdateID)
    {
      m_nFrame = rd->m_nFrameUpdateID;
      rd->m_RP.m_PS.m_NumVShaders++;
      if (pInst->m_nInstructions > rd->m_RP.m_PS.m_NumVSInstructions)
      {
        rd->m_RP.m_PS.m_NumVSInstructions = pInst->m_nInstructions;
        rd->m_RP.m_PS.m_pMaxVShader = this;
        rd->m_RP.m_PS.m_pMaxVSInstance = pInst;
      }
    }
    if (!(nFlags & HWSF_PRECACHE))
    {
      if (m_pCurVS != pInst->m_Handle.m_pShader)
      {
        m_pCurVS = pInst->m_Handle.m_pShader;
        rd->m_RP.m_PS.m_NumVShadChanges++;
        mfBind();
      }
#ifdef DIRECT3D10
      m_pCurInstVS = pInst;
#endif
      if (rd->m_RP.m_pRE)
        rd->m_RP.m_CurVFormat = pInst->m_nVertexFormat;
      rd->m_RP.m_FlagsStreams_Decl = pInst->m_VStreamMask_Decl;
      rd->m_RP.m_FlagsStreams_Stream = pInst->m_VStreamMask_Stream;

      // Make sure we don't use any texture attributes except baseTC in instancing case
      if (nFlags & HWSF_INSTANCED)
      {
        rd->m_RP.m_FlagsStreams_Decl &= ~(VSM_LMTC | VSM_MORPHBUDDY);
        rd->m_RP.m_FlagsStreams_Stream &= ~(VSM_LMTC | VSM_MORPHBUDDY);
      }

      mfSetParameters(0);
    }
  }
#if defined (DIRECT3D10)
  else
  if (m_eSHClass == eHWSC_Geometry)
  {
    m_pCurInstGS = pInst;
    if (!(nFlags & HWSF_PRECACHE))
    {
      mfBindGS(pInst->m_Handle.m_pShader, pInst->m_Handle.m_pShader->m_pHandle);

      mfSetParameters(0);
    }
  }
#endif

  return true;
}

//=======================================================================

/* returns a random floating point number between 0.0 and 1.0 */
static float frand()
{
  return (float) (rand() / (float) RAND_MAX);
}

/* returns a random floating point number between -1.0 and 1.0 */
static float sfrand()
{
  return (float) (rand() * 2.0f/ (float) RAND_MAX) - 1.0f;
}

void CHWShader_D3D::mfSetLightParams(int nPass)
{
  CD3D9Renderer *rd = gcpRendD3D;
  int nMaxLights = rd->m_RP.m_nMaxLightsPerPass;
  int i;

  SRenderShaderResources *pRes = rd->m_RP.m_pShaderResources;
  SLightPass *pLP = &rd->m_RP.m_LPasses[nPass];
  Vec3 vViewPos = rd->GetRCamera().Orig;
  Vec4 *pDstPS = NULL;
  Vec4 *pDstVS = NULL;

  if (rd->m_RP.m_nShaderQuality == eSQ_Low && pRes && pRes->m_Constants[eHWSC_Pixel].size() >= 2)
  for (i=0; i<pLP->nLights; i++)
  {
    CDLight *pDL = pLP->pLights[i];
    sData[0].f[0] = pDL->m_Color[0] * pRes->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][0];
    sData[0].f[1] = pDL->m_Color[1] * pRes->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][1];
    sData[0].f[2] = pDL->m_Color[2] * pRes->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][2];
    sData[0].f[3] = 1;

    Vec3 v = pDL->m_Origin - vViewPos;
    sData[1].f[0] = v.x;
    sData[1].f[1] = v.y;
    sData[1].f[2] = v.z;
    float fRadius = pDL->m_fRadius;
    if (fRadius <= 0)
      fRadius = 1.f;
    sData[1].f[3] = 1.f / fRadius;

    sData[2].f[0] = pDL->m_ShadowChanMask.x;
    sData[2].f[1] = pDL->m_ShadowChanMask.y;
    sData[2].f[2] = pDL->m_ShadowChanMask.z;
    sData[2].f[3] = pDL->m_ShadowChanMask.w;

    sData[3].f[0] = pDL->m_Color[0] * pDL->m_SpecMult * pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][0];
    sData[3].f[1] = pDL->m_Color[1] * pDL->m_SpecMult * pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][1];
    sData[3].f[2] = pDL->m_Color[2] * pDL->m_SpecMult * pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][2];
    sData[3].f[3] = 1;

    if (sData[3].f[0]>0.1f || sData[3].f[1]>0.1f || sData[3].f[2]>0.1f)
    {
      if (!rd->m_RP.m_nShaderQuality)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SPECULAR];
      sData[3].f[3] = max( 0.001f, pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL][3]);
    }

#ifndef DIRECT3D10
    if (CParserBin::m_bNewLightSetup)
    {
      mfParameterRegA(0, &sData[0].f[0], 4, eHWSC_Pixel);
      mfParameterRegA(4, &sData[1].f[0], 1, eHWSC_Vertex);
    }
    else
    {
      mfParameterRegA(0*nMaxLights+i, &sData[0].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(1*nMaxLights+i, &sData[1].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(2*nMaxLights+i, &sData[2].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(3*nMaxLights+i, &sData[3].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(4+i, &sData[1].f[0], 1, eHWSC_Vertex);
    }
#else
    if (!m_pLightCB[eHWSC_Pixel])
    {
      D3D10_BUFFER_DESC bd;
      ZeroStruct(bd);
      HRESULT hr;

      bd.Usage = D3D10_USAGE_DYNAMIC;
      bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
      bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      bd.MiscFlags = 0;
      bd.ByteWidth = 4*4 * sizeof(Vec4);
      hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pLightCB[eHWSC_Pixel]);
      assert(SUCCEEDED(hr));

      bd.ByteWidth = 4 * sizeof(Vec4);
      hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pLightCB[eHWSC_Vertex]);
      assert(SUCCEEDED(hr));
    }
    if (!pDstPS)
      m_pLightCB[eHWSC_Pixel]->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pDstPS);
    Vec4 *pSrc = (Vec4 *)&sData[0].f[0];
    if (CParserBin::m_bNewLightSetup)
    {
      pDstPS[0] = pSrc[0];
      pDstPS[1] = pSrc[1];
      pDstPS[2] = pSrc[2];
      pDstPS[3] = pSrc[3];
    }
    else
    {
      pDstPS[0*nMaxLights+i] = pSrc[0];
      pDstPS[1*nMaxLights+i] = pSrc[1];
      pDstPS[2*nMaxLights+i] = pSrc[2];
      pDstPS[3*nMaxLights+i] = pSrc[3];
    }

    if (!pDstVS)
      m_pLightCB[eHWSC_Vertex]->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pDstVS);
    pDstVS[i] = pSrc[1];
#endif
  }
  else
  for (i=0; i<pLP->nLights; i++)
  {
    CDLight *pDL = pLP->pLights[i];
    sData[0].f[0] = pDL->m_Color[0];
    sData[0].f[1] = pDL->m_Color[1];
    sData[0].f[2] = pDL->m_Color[2];
    sData[0].f[3] = pDL->m_SpecMult;

    Vec3 v = pDL->m_Origin - vViewPos;
    sData[1].f[0] = v.x;
    sData[1].f[1] = v.y;
    sData[1].f[2] = v.z;
    float fRadius = pDL->m_fRadius;
    if (fRadius <= 0)
      fRadius = 1.f;
    sData[1].f[3] = 1.f / fRadius;

    sData[2].f[0] = pDL->m_ShadowChanMask.x;
    sData[2].f[1] = pDL->m_ShadowChanMask.y;
    sData[2].f[2] = pDL->m_ShadowChanMask.z;
    sData[2].f[3] = pDL->m_ShadowChanMask.w;

#ifndef DIRECT3D10
    if (CParserBin::m_bNewLightSetup)
    {
      mfParameterRegA(i*3, &sData[0].f[0], 3, eHWSC_Pixel);
      mfParameterRegA(4+i, &sData[1].f[0], 1, eHWSC_Vertex);
    }
    else
    {
      mfParameterRegA(0*nMaxLights+i, &sData[0].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(1*nMaxLights+i, &sData[1].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(2*nMaxLights+i, &sData[2].f[0], 1, eHWSC_Pixel);
      mfParameterRegA(4+i, &sData[1].f[0], 1, eHWSC_Vertex);
    }
#else
    if (!m_pLightCB[eHWSC_Pixel])
    {
      D3D10_BUFFER_DESC bd;
      ZeroStruct(bd);
      HRESULT hr;

      bd.Usage = D3D10_USAGE_DYNAMIC;
      bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
      bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      bd.MiscFlags = 0;
      bd.ByteWidth = 3*4 * sizeof(Vec4);
      hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pLightCB[eHWSC_Pixel]);
      assert(SUCCEEDED(hr));

      bd.ByteWidth = 4 * sizeof(Vec4);
      hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pLightCB[eHWSC_Vertex]);
      assert(SUCCEEDED(hr));
    }
    if (!pDstPS)
      m_pLightCB[eHWSC_Pixel]->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pDstPS);
    Vec4 *pSrc = (Vec4 *)&sData[0].f[0];
    if (CParserBin::m_bNewLightSetup)
    {
      pDstPS[i*3+0] = pSrc[0];
      pDstPS[i*3+1] = pSrc[1];
      pDstPS[i*3+2] = pSrc[2];
    }
    else
    {
      pDstPS[0*nMaxLights+i] = pSrc[0];
      pDstPS[1*nMaxLights+i] = pSrc[1];
      pDstPS[2*nMaxLights+i] = pSrc[2];
    }
    if (!pDstVS)
      m_pLightCB[eHWSC_Vertex]->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pDstVS);
    pDstVS[i] = pSrc[1];
#endif
  }
#ifdef DIRECT3D10
  if (pDstPS)
  {
    m_pLightCB[eHWSC_Pixel]->Unmap();
    m_pCurReqCB[eHWSC_Pixel][CB_PER_LIGHT] = m_pLightCB[eHWSC_Pixel];
  }
  if (pDstVS)
  {
    m_pLightCB[eHWSC_Vertex]->Unmap();
    m_pCurReqCB[eHWSC_Vertex][CB_PER_LIGHT] = m_pLightCB[eHWSC_Vertex];
  }
#endif
}

void CHWShader_D3D::mfSetCM()
{
#if defined (DIRECT3D10)
  if (!m_CM_Params[eHWSC_Pixel].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Pixel][0], m_CM_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel, m_nMax_PF_Vecs[eHWSC_Pixel]);
  }
  if (!m_CM_Params[eHWSC_Vertex].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Vertex][0], m_CM_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex, m_nMax_PF_Vecs[eHWSC_Vertex]);
  }
  if (!m_CM_Params[eHWSC_Geometry].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Geometry][0], m_CM_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry, m_nMax_PF_Vecs[eHWSC_Geometry]);
  }
#else
  if (!m_CM_Params[eHWSC_Pixel].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Pixel][0], m_CM_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel);
  }
  if (!m_CM_Params[eHWSC_Vertex].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Vertex][0], m_CM_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex);
  }
  if (!m_CM_Params[eHWSC_Geometry].empty())
  {
    mfSetParameters(&m_CM_Params[eHWSC_Geometry][0], m_CM_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry);
  }
#endif
}

void CHWShader_D3D::mfSetPF()
{
  CD3D9Renderer *r = gcpRendD3D;

#if defined (DIRECT3D10)
  if (!m_PF_Params[eHWSC_Pixel].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Pixel][0], m_PF_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel, m_nMax_PF_Vecs[eHWSC_Pixel]);
  }
  if (!m_PF_Params[eHWSC_Vertex].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Vertex][0], m_PF_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex, m_nMax_PF_Vecs[eHWSC_Vertex]);
  }
  if (!m_PF_Params[eHWSC_Geometry].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Geometry][0], m_PF_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry, m_nMax_PF_Vecs[eHWSC_Geometry]);
  }
  if (r->m_RP.m_PersFlags & RBPF_SHADOWGEN)
  {
    if (!m_SG_Params[eHWSC_Pixel].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Pixel][0], m_SG_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel, m_nMax_SG_Vecs[eHWSC_Pixel]);
    }
    if (!m_SG_Params[eHWSC_Vertex].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Vertex][0], m_SG_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex, m_nMax_SG_Vecs[eHWSC_Vertex]);
    }
    if (!m_SG_Params[eHWSC_Geometry].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Geometry][0], m_SG_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry, m_nMax_SG_Vecs[eHWSC_Geometry]);
    }
  }
#else
  if (!m_PF_Params[eHWSC_Pixel].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Pixel][0], m_PF_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel);
  }
  if (!m_PF_Params[eHWSC_Vertex].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Vertex][0], m_PF_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex);
  }
  if (!m_PF_Params[eHWSC_Geometry].empty())
  {
    mfSetParameters(&m_PF_Params[eHWSC_Geometry][0], m_PF_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry);
  }
  if (r->m_RP.m_PersFlags & RBPF_SHADOWGEN)
  {
    if (!m_SG_Params[eHWSC_Pixel].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Pixel][0], m_SG_Params[eHWSC_Pixel].size(), NULL, eHWSC_Pixel);
    }
    if (!m_SG_Params[eHWSC_Vertex].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Vertex][0], m_SG_Params[eHWSC_Vertex].size(), NULL, eHWSC_Vertex);
    }
    if (!m_SG_Params[eHWSC_Geometry].empty())
    {
      mfSetParameters(&m_SG_Params[eHWSC_Geometry][0], m_SG_Params[eHWSC_Geometry].size(), NULL, eHWSC_Geometry);
    }
  }
#endif
}

void CHWShader_D3D::mfSetGlobalParams()
{
  UpdateConstParamsPF();

  if (CRenderer::CV_r_log >= 3)
    gRenDev->Logv(SRendItem::m_RecurseLevel, "--- Set global shader constants...\n");
#if defined(OPENGL)
  static bool globalMapBuilt = false;//tracks if global has been built
  if(!globalMapBuilt)//create only once
  {
    for(int i=0; i<scSGlobalConstMapCount; ++i)
      sGlobalConsts[i].Init();
#if 0
    sGlobalConsts[VSCONST_FOG_OPENGL].shaderType			= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_FOG_OPENGL].cpConstName			= "_g_VSFog";
#endif

#if 0
    sGlobalConsts[PSCONST_FOGCOLOR_OPENGL].shaderType			= SGlobalConstMap::scPSConst;
    sGlobalConsts[PSCONST_FOGCOLOR_OPENGL].cpConstName		= "_g_PSFogColor";
#endif

    sGlobalConsts[VSCONST_0_025_05_1_OPENGL].shaderType			= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_0_025_05_1_OPENGL].cpConstName		= "_g_VSConsts0";

    //init instancing stuff
    sGlobalConsts[VSCONST_INSTDATA_OPENGL].shaderType		= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_INSTDATA_OPENGL].cpConstName	= "_g_InstData";
    //skinning quats and shape deformation stuff
#if 0
    sGlobalConsts[VSCONST_SKINMATRIX_OPENGL].shaderType		= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_SKINMATRIX_OPENGL].cpConstName	= "_g_SkinMatrices";
    sGlobalConsts[VSCONST_SKINMATRIX_OPENGL].isMatrix			= scIs3x4Matrix;
#endif
    sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].shaderType		= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].cpConstName		= "_g_SkinQuatS"; // same as "_g_SkinQuatL"
    sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].isMatrix			= scIs2x4Matrix;
    sGlobalConsts[VSCONST_SHAPEDEFORMATION_OPENGL].shaderType		= SGlobalConstMap::scVSConst;
    sGlobalConsts[VSCONST_SHAPEDEFORMATION_OPENGL].cpConstName	= "_g_ShapeDeformationData";

    globalMapBuilt = true;
  }
#endif

  Vec4 v;
  CD3D9Renderer *r = gcpRendD3D;

#if defined(DIRECT3D10)
  // Preallocate global constant buffer arrays
  if (!m_pCB[eHWSC_Vertex][CB_PER_BATCH])
  {
    int i, j;
    for (i=0; i<CB_MAX; i++)
    {
      for (j=0; j<eHWSC_Max; j++)
      {
#if !defined(PS3)
        int nSize;
        switch (j)
        {
          case eHWSC_Pixel:
            nSize = MAX_CONSTANTS_PS;
            break;
          case eHWSC_Vertex:
            nSize = MAX_CONSTANTS_VS;
            break;
          case eHWSC_Geometry:
            nSize = MAX_CONSTANTS_GS;
            break;
          default:
            assert(0);
            break;
        }
#else
				int nSize	=	MAX_CONSTANTS;
#endif

        m_pCB[j][i] = new ID3D10Buffer* [nSize];
        memset(m_pCB[j][i], 0, sizeof(ID3D10Buffer*)*(nSize));

				m_pCBShadow[j][i] = new void* [nSize];
				memset(m_pCBShadow[j][i], 0, sizeof(void*)*(nSize));
			}
    }
  }
#endif

#if defined (DIRECT3D10)
	gRenDev->m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF | RBPF2_COMMIT_CM;
#else
	gRenDev->m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
#endif
}

void CHWShader_D3D::mfSetCameraParams()
{
  if (CRenderer::CV_r_log >= 3)
    gRenDev->Logv(SRendItem::m_RecurseLevel, "--- Set camera shader constants...\n");

#if defined (DIRECT3D10)
	gRenDev->m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF | RBPF2_COMMIT_CM;
#else
	gRenDev->m_RP.m_PersFlags2 |= RBPF2_COMMIT_CM;
#endif
}

bool CHWShader_D3D::mfAddGlobalParameter(SCGParam& Param, EHWShaderClass eSH, bool bSG, bool bCam)
{
  int i;

  if (bCam)
  {
    for (i=0; i<m_CM_Params[eSH].size(); i++)
    {
      SCGParam *pP = &m_CM_Params[eSH][i];
      if (pP->m_Name == Param.m_Name)
        break;
    }
    if (i == m_CM_Params[eSH].size())
    {
      m_CM_Params[eSH].push_back(Param);
#if defined (DIRECT3D10)
      m_nMax_PF_Vecs[eSH] = max((Param.m_dwBind>>2)+((Param.m_nParameters+3)>>2), m_nMax_PF_Vecs[eSH]);
#endif
      return true;
    }
  }
  else
  if (!bSG)
  {
    for (i=0; i<m_PF_Params[eSH].size(); i++)
    {
      SCGParam *pP = &m_PF_Params[eSH][i];
      if (pP->m_Name == Param.m_Name)
        break;
    }
    if (i == m_PF_Params[eSH].size())
    {
      m_PF_Params[eSH].push_back(Param);
  #if defined (DIRECT3D10)
      m_nMax_PF_Vecs[eSH] = max((Param.m_dwBind>>2)+((Param.m_nParameters+3)>>2), m_nMax_PF_Vecs[eSH]);
  #endif
      return true;
    }
  }
  else
  {
    for (i=0; i<m_SG_Params[eSH].size(); i++)
    {
      SCGParam *pP = &m_SG_Params[eSH][i];
      if (pP->m_Name == Param.m_Name)
        break;
    }
    if (i == m_SG_Params[eSH].size())
    {
      m_SG_Params[eSH].push_back(Param);
#if defined (DIRECT3D10)
      m_nMax_SG_Vecs[eSH] = max((Param.m_dwBind>>2)+((Param.m_nParameters+3)>>2), m_nMax_SG_Vecs[eSH]);
#endif
      return true;
    }
  }
  return false;
}

uint CHWShader_D3D::mfGetPreprocessFlags(SShaderTechnique *pTech)
{
  uint i, j;
  uint nFlags = 0;

  for (i=0; i<m_Insts.Num(); i++)
  {
    SHWSInstance *pInst = &m_Insts[i];
    if (pInst->m_pSamplers)
    {
      for (j=0; j<pInst->m_pSamplers->size(); j++)
      {
        STexSampler *pSamp = &(*pInst->m_pSamplers)[j];
        if (pSamp && pSamp->m_pTarget)
        {
          SHRenderTarget *pTarg = pSamp->m_pTarget;
          if (pTarg->m_eOrder == eRO_PreProcess)
            nFlags |= pTarg->m_nProcessFlags;
          if (pTech)
          {
            uint n = 0;
            for (n=0; n<pTech->m_RTargets.Num(); n++)
            {
              if (pTarg == pTech->m_RTargets[n])
                break;
            }
            if (n == pTech->m_RTargets.Num())
              pTech->m_RTargets.AddElem(pTarg);
          }
        }
      }
    }
  }
  if (pTech)
    pTech->m_RTargets.Shrink();

  return nFlags;
}

const char * CHWShader_D3D::mfGetSharedActivatedCombinations()
{
  TArray <char> Combinations;

  int i, j;
  InstanceMapItor it;
  for (it=m_SharedInsts.begin(); it!=m_SharedInsts.end(); it++)
  {
    SHWSSharedList *pList = it->second;
    CCryName Name = it->first;
    if (!pList)
      continue;
    for (i=0; i<pList->m_SharedInsts.Num(); i++)
    {
      SHWSSharedInstance *pSInst = &pList->m_SharedInsts[i];
      for (j=0; j<pSInst->m_Insts.Num(); j++)
      {
        SHWSInstance *pInst = &pSInst->m_Insts[j];
        char name[256];
        sprintf(name, "%s@%s", pList->m_SharedNames[0].m_Name.c_str(), Name.c_str());
        const char *str = gRenDev->m_cEF.mfInsertNewCombination(pSInst->m_GLMask, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, name, false);
        assert (str);
        if (str)
        {
          Combinations.Copy(str, strlen(str));
          Combinations.Copy("\n", 1);
        }
      }
    }
  }
  if (!Combinations.Num())
    return NULL;
  char *pPtr = new char [Combinations.Num()+1];
  memcpy(pPtr, &Combinations[0], Combinations.Num());
  pPtr[Combinations.Num()] = 0;
  return pPtr;
}

const char * CHWShader_D3D::mfGetActivatedCombinations()
{
  TArray <char> Combinations;
  char *pPtr = NULL;
  int i;

  for (i=0; i<m_Insts.Num(); i++)
  {
    SHWSInstance *pInst = &m_Insts[i];
    char name[256];
    strcpy(name, GetName());
    char *s = strchr(name, '(');
    if (s)
      s[0] = 0;
    const char *str = gRenDev->m_cEF.mfInsertNewCombination(m_nMaskGenFX, pInst->m_RTMask, pInst->m_LightMask, pInst->m_MDMask, pInst->m_MDVMask, pInst->m_eProfileType, name, false);
    assert (str);
    if (str)
    {
      Combinations.Copy(str, strlen(str));
      Combinations.Copy("\n", 1);
    }
  }

  if (!Combinations.Num())
    return NULL;
  pPtr = new char [Combinations.Num()+1];
  memcpy(pPtr, &Combinations[0], Combinations.Num());
  pPtr[Combinations.Num()] = 0;
  return pPtr;
}

const char *CHWShader::GetCurrentShaderCombinations(void)
{
  TArray <char> Combinations;
  char *pPtr = NULL;
  CCryName Name;
  SResourceContainer *pRL;

  Name = CHWShader::mfGetClassName(eHWSC_Vertex);
  pRL = CBaseResource::GetResourcesForClass(Name);
  int nVS = 0;
  int nPS = 0;
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CHWShader *vs = (CHWShader *)itor->second;
      if (!vs)
        continue;
      const char *szCombs = vs->mfGetActivatedCombinations();
      if (!szCombs)
        continue;
      Combinations.Copy(szCombs, strlen(szCombs));
      delete [] szCombs;
      nVS++;
    }
  }

  Name = CHWShader::mfGetClassName(eHWSC_Pixel);
  pRL = CBaseResource::GetResourcesForClass(Name);
  int n = 0;
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CHWShader *ps = (CHWShader *)itor->second;
      if (!ps)
        continue;
      const char *szCombs = ps->mfGetActivatedCombinations();
      if (!szCombs)
        continue;
      Combinations.Copy(szCombs, strlen(szCombs));
      delete [] szCombs;
      nPS++;
    }
  }

  const char *szCombs = CHWShader_D3D::mfGetSharedActivatedCombinations();
  if (szCombs)
  {
    Combinations.Copy(szCombs, strlen(szCombs));
    delete [] szCombs;
  }

  if (!Combinations.Num())
    return NULL;
  pPtr = new char [Combinations.Num()+1];
  memcpy(pPtr, &Combinations[0], Combinations.Num());
  pPtr[Combinations.Num()] = 0;
  return pPtr;
}

bool CHWShader::SetCurrentShaderCombinations(const char *szCombinations)
{
  bool bRes = true;

  FXShaderCacheCombinations Combinations;
  gRenDev->m_cEF.mfInitShadersCache(&Combinations, szCombinations);
  if (!Combinations.size())
    return false;
  gRenDev->m_cEF.mfPrecacheShaders(&Combinations);

  return bRes;
}

//////////////////////////////////////////////////////////////////////////

#if defined (WIN32) || defined(XENON)

#pragma warning(disable: 4355) // warning C4355: 'this' : used in base member initializer list

CAsyncShaderManager::CAsyncShaderManager() : m_thread(this)
{

}

void CAsyncShaderManager::InsertPendingShader(SShaderAsyncInfo* pAsync)
{
	CScopedLock lock(m_lock);
	pAsync->Link(&m_build_list);
	InterlockedIncrement(&SShaderAsyncInfo::s_nPendingAsyncShaders);
}

void CAsyncShaderManager::FlushPendingShaders()
{
	{
		CScopedLock lock(m_lock);
		assert(m_flush_list.m_Prev == &m_flush_list && m_flush_list.m_Next == &m_flush_list); // the flush list must be empty - cleared by the shader compile thread
		if (m_build_list.m_Prev == &m_build_list && m_build_list.m_Next == &m_build_list)
			return; // the build list is empty, might need to do some assert here
		m_flush_list.m_Prev = m_build_list.m_Prev;
		m_flush_list.m_Next = m_build_list.m_Next;
		m_build_list.m_Next->m_Prev = &m_flush_list;
		m_build_list.m_Prev->m_Next = &m_flush_list;
		m_build_list.m_Prev = &m_build_list;
		m_build_list.m_Next = &m_build_list;
	}

	SShaderAsyncInfo *pAI, *pAI2, *pAINext;

	// Sorting by distance
	for (pAI=m_flush_list.m_Next; pAI!=&m_flush_list; pAI=pAI->m_Next)
	{
		pAINext = NULL;
		int nFrame = pAI->m_nFrame;
		float fDist = pAI->m_fMinDistance;
		for (pAI2=pAI->m_Next; pAI2!=&m_flush_list; pAI2=pAI2->m_Next)
		{
			if (pAI2->m_nFrame < nFrame)
				continue;
			if (pAI2->m_nFrame > nFrame || pAI2->m_fMinDistance < fDist)
			{
				pAINext = pAI2;
				nFrame = pAI2->m_nFrame;
				fDist = pAI2->m_fMinDistance;
			}
		}
		if (pAINext)
		{
			assert(pAI != pAINext);
			SShaderAsyncInfo *pAIP0 = pAI->m_Prev;
			SShaderAsyncInfo *pAIP1 = pAINext->m_Prev == pAI ? pAINext : pAINext->m_Prev;

			pAI->m_Next->m_Prev = pAI->m_Prev;
			pAI->m_Prev->m_Next = pAI->m_Next;
			pAI->m_Next = pAIP1->m_Next;
			pAIP1->m_Next->m_Prev = pAI;
			pAIP1->m_Next = pAI;
			pAI->m_Prev = pAIP1;

			pAI = pAINext;

			pAI->m_Next->m_Prev = pAI->m_Prev;
			pAI->m_Prev->m_Next = pAI->m_Next;
			pAI->m_Next = pAIP0->m_Next;
			pAIP0->m_Next->m_Prev = pAI;
			pAIP0->m_Next = pAI;
			pAI->m_Prev = pAIP0;
		}
	}

	for (pAI=m_flush_list.m_Next; pAI!=&m_flush_list; pAI=pAINext)
	{
		pAINext = pAI->m_Next;
		assert(pAI->m_bPending);
		CompileAsyncShader(pAI);
		pAI->Unlink();
		pAI->m_bPending = false;
		InterlockedDecrement(&SShaderAsyncInfo::s_nPendingAsyncShaders);
	}
}

void CAsyncShaderManager::CompileAsyncShader(SShaderAsyncInfo* pAsync)
{
#if defined (DIRECT3D9) || defined(OPENGL)
	int nFlags = D3DXSHADER_PACKMATRIX_ROWMAJOR;
#ifdef XENON
	nFlags |= D3DXSHADER_MICROCODE_BACKEND_NEW;
#endif

	HRESULT hr = D3DXCompileShader(pAsync->m_Text.c_str(), pAsync->m_Text.size(), NULL, NULL, pAsync->m_Name.c_str(), pAsync->m_Profile, nFlags, &pAsync->m_pDevShader, &pAsync->m_pErrors, &pAsync->m_pConstants); 
	if (FAILED(hr))
	{
		if (pAsync->m_pErrors)
		{
			const char *err = (const char *)pAsync->m_pErrors->GetBufferPointer();
			pAsync->m_Errors += err;
		}
		else
		{ 
			pAsync->m_Errors += "D3DXCompileShader failed";
		}
	}
	else
	{
		SAFE_RELEASE(pAsync->m_pErrors);
		CHWShader_D3D *pSH = pAsync->m_pShader;
		CHWShader_D3D::SHWSInstance *pInst = pSH->mfGetInstance(pAsync->m_nOwner, pSH->m_nMaskGenShader);
		bool bResult = CHWShader_D3D::mfCreateShaderEnv(pInst, pAsync->m_pDevShader, pAsync->m_pConstants, pAsync->m_pErrors, pAsync->m_InstBindVars, pSH, true);
		assert(bResult == true);
	}
#elif defined (DIRECT3D10)
	const char *Name = pAsync->m_pShader ? pAsync->m_pShader->GetName() : "Unknown";
	HRESULT hr = S_OK;
	hr = D3DX10CompileFromMemory(pAsync->m_Text.c_str(),
		pAsync->m_Text.size(),
		Name,
		NULL,
		NULL,
		pAsync->m_Name.c_str(),
		pAsync->m_Profile.c_str(),
		D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY,
		0,
		NULL,
		(ID3D10Blob **)&pAsync->m_pDevShader,
		(ID3D10Blob **)&pAsync->m_pErrors, &hr);
	if (FAILED(hr) || !pAsync->m_pDevShader)
	{
		if (pAsync->m_pErrors)
		{
			const char *err = (const char *)pAsync->m_pErrors->GetBufferPointer();
			pAsync->m_Errors += err;
		}
		else
		{
			pAsync->m_Errors += "D3DXCompileShader failed";
		}
	}
	else
	{
		ID3D10ShaderReflection *pShaderReflection;
		UINT *pData = (UINT *)pAsync->m_pDevShader->GetBufferPointer();
		UINT nSize = pAsync->m_pDevShader->GetBufferSize();
		hr = D3D10ReflectShader(pData, nSize, &pShaderReflection);
		if (SUCCEEDED(hr))
		{
			pAsync->m_pConstants = (LPD3DXCONSTANTTABLE)pShaderReflection;
			CHWShader_D3D *pSH = pAsync->m_pShader;
			CHWShader_D3D::SHWSInstance *pInst = pSH->mfGetInstance(pAsync->m_nOwner, pSH->m_nMaskGenShader);
			bool bResult = CHWShader_D3D::mfCreateShaderEnv(pInst, pAsync->m_pDevShader, pAsync->m_pConstants, pAsync->m_pErrors, pAsync->m_InstBindVars, pSH, true);
			assert(bResult == true);
		}
		else
		{
			assert(0);
		}
	}
#endif
}

void CAsyncShaderManager::CShaderThread::Run()
{
	CryThreadSetName( -1, "ShaderCompile" );		

	while (!m_quit)
	{
		m_man->FlushPendingShaders();

    if (!CRenderer::CV_r_shadersasynccompiling)
		  Sleep(250);
    else
      Sleep(5);
	}
}

#endif
