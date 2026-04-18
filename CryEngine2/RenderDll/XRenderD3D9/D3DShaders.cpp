/*=============================================================================
  D3DShaders.cpp : Direct3D specific effectors/shaders functions implementation.
  Copyright 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"

//==================================================================================

bool CShader::FXSetTechnique(const CCryName& Name)
{
  uint i;
  SShaderTechnique *pTech = NULL;
  for (i=0; i<m_HWTechniques.Num(); i++)
  {
    pTech = m_HWTechniques[i];
    if (Name == pTech->m_Name)
      break;
  }
  if (i == m_HWTechniques.Num())
    return false;

  CRenderer *rd = gRenDev;
  rd->m_RP.m_pShader = this;
  rd->m_RP.m_pCurTechnique = pTech;

  return true;
}

bool CShader::FXSetPSFloat(const CCryName& NameParam, const Vec4* fParams, int nParams)
{
  CRenderer *rd = gRenDev;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
    return false;
  SShaderPass *pPass = rd->m_RP.m_pCurPass;
  if (!pPass)
    return false;
  CHWShader_D3D *curPS = (CHWShader_D3D *)pPass->m_PShader;
  if (!curPS)
    return false;
  SCGBind *pBind = curPS->mfGetParameterBind(NameParam);
  if (!pBind)
    return false;
#if defined (DIRECT3D9) || defined (OPENGL)
  curPS->mfParameterf(pBind, &fParams[0].x, eHWSC_Pixel);
#elif defined (DIRECT3D10)
  curPS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Pixel, &fParams[0].x, nParams*4, curPS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
#endif
  return true;
}

bool CShader::FXSetVSFloat(const CCryName& NameParam, const Vec4* fParams, int nParams)
{
  CRenderer *rd = gRenDev;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
    return false;
  SShaderPass *pPass = rd->m_RP.m_pCurPass;
  if (!pPass)
    return false;
  CHWShader_D3D *curVS = (CHWShader_D3D *)pPass->m_VShader;
  if (!curVS)
    return false;
  SCGBind *pBind = curVS->mfGetParameterBind(NameParam);
  if (!pBind)
    return false;
#if defined (DIRECT3D9) || defined (OPENGL)
  curVS->mfParameterf(pBind, &fParams[0].x, eHWSC_Vertex);
#elif defined (DIRECT3D10)
  curVS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Vertex, &fParams[0].x, nParams*4, curVS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
#endif
  return true;
}

bool CShader::FXBegin(uint *uiPassCount, uint nFlags)
{
  CRenderer *rd = gRenDev;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
    return false;
  *uiPassCount = rd->m_RP.m_pCurTechnique->m_Passes.Num();
  rd->m_RP.m_nFlagsShaderBegin = nFlags;
  rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[0];
  return true;
}

bool CShader::FXBeginPass(uint uiPass)
{
  CD3D9Renderer *rd = gcpRendD3D;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
    return false;
  rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[uiPass];
  SShaderPass *pPass = rd->m_RP.m_pCurPass;
  assert (pPass->m_VShader && pPass->m_PShader);
  if (!pPass->m_VShader || !pPass->m_PShader)
    return false;
  CHWShader *curVS = pPass->m_VShader;
  CHWShader *curPS = pPass->m_PShader;
#if defined (DIRECT3D10)
  CHWShader *curGS = pPass->m_GShader;
#endif

  bool bResult = true;

  // Set Pixel shader and all associated textures
  if (rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETTEXTURES)
    bResult &= curPS->mfSet(0);
  else
    bResult &= curPS->mfSet(HWSF_SETTEXTURES);
  // Set Vertex shader
  curVS->mfSet(0);

#if defined (DIRECT3D10)
  // Set Geometry shader
  if (curGS)
    bResult &= curGS->mfSet(0);
  else
    CHWShader_D3D::mfBindGS(NULL, NULL);
#endif

  if (!(rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETSTATES))
  {
    rd->EF_SetState(pPass->m_RenderState);
    if (pPass->m_eCull != -1)
      rd->D3DSetCull((ECull)pPass->m_eCull);
  }

  curVS->mfSetParameters(1);
  curPS->mfSetParameters(1);

  return bResult;
}

bool CShader::FXEndPass()
{
  CRenderer *rd = gRenDev;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
    return false;
  rd->m_RP.m_pCurPass = NULL;
  return true;
}

bool CShader::FXEnd()
{
  CRenderer *rd = gRenDev;
  if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
    return false;
  return true;
}

bool CShader::FXCommit(const uint nFlags)
{
  gcpRendD3D->EF_Commit();

  return true;
}


//===================================================================================

bool CD3D9Renderer::FX_SetFPMode()
{
  if (!(m_RP.m_PersFlags & (RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY)) && CShaderMan::m_ShaderFPEmu == m_RP.m_pShader)
    return true;
  m_RP.m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
  m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
  m_RP.m_pCurObject = m_RP.m_TempObjects[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  CShader *pSh = CShaderMan::m_ShaderFPEmu;
  if (!pSh || !pSh->m_HWTechniques.Num())
    return false;
  m_RP.m_FlagsShader_LT = m_eCurColorOp[0] | (m_eCurAlphaOp[0] << 8) | (m_eCurColorArg[0] << 16) | (m_eCurAlphaArg[0] << 24);
  if (CTexture::m_TexStages[0].m_Texture && CTexture::m_TexStages[0].m_Texture->GetTextureType() == eTT_Cube)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
  else
    m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_CUBEMAP0];

  m_RP.m_pShader = pSh;
  m_RP.m_pCurTechnique = pSh->m_HWTechniques[0];
  bool bRes = pSh->FXBegin(&m_RP.m_nNumRendPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  if (!bRes)
    return false;
  bRes = pSh->FXBeginPass(0);
  EF_Commit();
  return bRes;
}


void CShaderMan::mfCheckObjectDependParams(std::vector<SCGParam>* PNoObj, std::vector<SCGParam>*& PObj, EHWShaderClass eSH)
{
  if (!PNoObj)
    return;
  uint i;
  for (i=0; i<PNoObj->size(); i++)
  {
    SCGParam *prNoObj = &(*PNoObj)[i];

    if (prNoObj->m_Flags & PF_INSTANCE)
    {
      if (!PObj)
        PObj = new std::vector<SCGParam>;
      PObj->push_back((*PNoObj)[i]);
      std::vector<SCGParam>::iterator it = PNoObj->begin()+i;
      PNoObj->erase(it);
      i--;
    }
    else
    if (prNoObj->m_Flags & PF_MATERIAL)
    {
#ifdef DIRECT3D10
      std::vector<SCGParam>::iterator it = PNoObj->begin()+i;
      PNoObj->erase(it);
      i--;
#endif
    }
    else
    if (prNoObj->m_Flags & PF_LIGHT)
    {
      std::vector<SCGParam>::iterator it = PNoObj->begin()+i;
      PNoObj->erase(it);
      i--;
    }
    else
    if (prNoObj->m_Flags & (PF_GLOBAL | PF_SHADOWGEN))
    {
      bool bCam = (prNoObj->m_eCGParamType == ECGP_Matr_PF_ViewProjMatrix || prNoObj->m_eCGParamType == ECGP_Matr_PF_ViewProjZeroMatrix);
      CHWShader_D3D::mfAddGlobalParameter((*PNoObj)[i], eSH, (prNoObj->m_Flags & PF_SHADOWGEN)!=0, bCam);
      std::vector<SCGParam>::iterator it = PNoObj->begin()+i;
      PNoObj->erase(it);
      i--;
    }
  }
  if (PObj)
  {
    int n = -1;
    for (i=0; i<PObj->size(); i++)
    {
      if ((*PObj)[i].m_eCGParamType == ECGP_Matr_PI_ViewProj)
      {
        n = i;
        break;
      }
    }
    if (n > 0)
    {
      SCGParam pr = (*PObj)[n];
      (*PObj)[n] = (*PObj)[0];
      (*PObj)[0] = pr;
    }
  }
}

static char *sSetParameterExp(char *szExpr, Vec4& vVal, SRenderShaderResources *pRes, bool& bResult);

static char *sSetOperand(char *szExpr, Vec4& vVal, SRenderShaderResources *pRes, bool& bResult)
{
  char temp[256];
  SkipCharacters(&szExpr, kWhiteSpace);
  if (*szExpr == '(')
  {
    szExpr = sSetParameterExp(szExpr, vVal, pRes, bResult);
  }
  else
  {
    fxFillParam(&szExpr, temp);
    if (temp[0] != '.' && (temp[0]<'0' || temp[0]>'9'))
    {
      bResult &= SShaderParam::GetValue(temp, &pRes->m_ShaderParams, &vVal[0], 0);
      //assert(bF);
    }
    else
      vVal[0] = shGetFloat(temp);
  }
  return szExpr;
}

static char *sSetParameterExp(char *szExpr, Vec4& vVal, SRenderShaderResources *pRes, bool& bResult)
{
  if (szExpr[0] != '(')
    return NULL;
  char expr[1024];
  int n = 0;
  int nc = 0;
  char theChar;
  while (theChar = *szExpr)
  {
    if (theChar == '(')
    {
      n++;
      if (n == 1)
      {
        szExpr++;
        continue;
      }
    }
    else
    if (theChar == ')')
    {
      n--;
      if (!n)
      {
        szExpr++;
        break;
      }
    }
    expr[nc++] = theChar;
    szExpr++;
  }
  expr[nc++] = 0;
  assert(!n);
  if (n)
    return NULL;

  char *szE = expr;
  Vec4 vVals[2];
  vVals[0] = Vec4(0,0,0,0);
  szE = sSetOperand(szE, vVals[0], pRes, bResult);
  SkipCharacters(&szE, kWhiteSpace);
  char szOp = *szE++;
  vVals[1] = Vec4(0,0,0,0);
  SkipCharacters(&szE, kWhiteSpace);
  szE = sSetOperand(szE, vVals[1], pRes, bResult);

  switch (szOp)
  {
    case '+':
      vVal[0] = vVals[0][0] + vVals[1][0];
      break;
    case '-':
      vVal[0] = vVals[0][0] - vVals[1][0];
      break;
    case '*':
      vVal[0] = vVals[0][0] * vVals[1][0];
      break;
    case '/':
      vVal[0] = vVals[0][0]; 
      if( vVals[1][0] )
        vVal[0] /= vVals[1][0];
      break;
  }

  return (char *)szExpr;
}

static bool sSetParameter(SFXParam *pPR, DynArray<Vec4>& Constants, SRenderShaderResources *pRes, int nOffs, EHWShaderClass eSHClass)
{
  uint nFlags = pPR->GetParamFlags();
  bool bFound = false;
  for (int i=0; i<4; i++)
  {
    if (nFlags & PF_AUTOMERGED)
    {
      string n = pPR->GetCompName(i);
      bool bF = SShaderParam::GetValue(n.c_str(), &pRes->m_ShaderParams, &Constants[pPR->m_nRegister[eSHClass]-nOffs][0], i);
      if (!bF)
      {
        string v = pPR->GetParamComp(i);
        if (v[0] == '(')
        {
          Vec4 vVal = Vec4(-10000, -10000, -10000, -10000);
          bool bResult = true;
          sSetParameterExp((char *)v.c_str(), vVal, pRes, bResult);
          if (bResult)
          {
            Constants[pPR->m_nRegister[eSHClass]-nOffs][i] = vVal[0];
            if (vVal[0] != -10000)
              bF = true;
          }
        }
      }
      bFound |= bF;
    }
    else
      bFound |= SShaderParam::GetValue(pPR->m_Name.c_str(), &pRes->m_ShaderParams, &Constants[pPR->m_nRegister[eSHClass]-nOffs][0], i);
  }
  return bFound;
}


inline bool CompareItem(const SFXParam *a, const SFXParam *b)
{
  return (a->m_nRegister[gRenDev->m_RP.m_FlagsShader_LT] < b->m_nRegister[gRenDev->m_RP.m_FlagsShader_LT]);
}

void SRenderShaderResources::UpdateConstants(IShader *pISH)
{
  int i, j, n, m;

  CShader *pSH = (CShader *)pISH;

  std::vector<SFXParam *> VSParams;
  std::vector<SFXParam *> PSParams;
  std::vector<SFXParam *> GSParams;

  byte bMergeMaskPS = 1;
  byte bMergeMaskVS = 2;
  byte bMergeMaskGS = 4;

  bool bHasGS = false;
  for (i=0; i<pSH->m_HWTechniques.Num(); i++)
  {
    SShaderTechnique *pTech = pSH->m_HWTechniques[i];
    for (j=0; j<pTech->m_Passes.Num(); j++)
    {
      SShaderPass *pPass = &pTech->m_Passes[j];
      CHWShader_D3D *pVS = (CHWShader_D3D *)pPass->m_VShader;
      CHWShader_D3D *pPS = (CHWShader_D3D *)pPass->m_PShader;
      CHWShader_D3D *pGS = (CHWShader_D3D *)pPass->m_GShader;
      assert(pVS && pPS);
      if (pVS)
      {
        for (n=0; n<pVS->m_Params.size(); n++)
        {
          SFXParam *pPR = &pVS->m_Params[n];
          if (pPR->m_bWasMerged & (bMergeMaskVS << 6))
            continue;
          if (pPR->m_nCB == CB_PER_MATERIAL)
          {
            if(pPR->m_nRegister[eHWSC_Vertex]<FIRST_REG_PM[eHWSC_Vertex] || pPR->m_nRegister[eHWSC_Vertex]>=10000)
              continue;
            for (m=0; m<VSParams.size(); m++)
            {
              if (VSParams[m]->m_Name == pPR->m_Name)
                break;
            }
            if (m == VSParams.size())
              VSParams.push_back(pPR);
          }
        }
      }

      if (pPS)
      {
        for (n=0; n<pPS->m_Params.size(); n++)
        {
          SFXParam *pPR = &pPS->m_Params[n];
          if (pPR->m_bWasMerged & (bMergeMaskPS << 6))
            continue;
          if (pPR->m_nCB == CB_PER_MATERIAL)
          {
            if(pPR->m_nRegister[eHWSC_Pixel]<FIRST_REG_PM[eHWSC_Pixel] || pPR->m_nRegister[eHWSC_Pixel]>=10000)
              continue;
            for (m=0; m<PSParams.size(); m++)
            {
              if (PSParams[m]->m_Name == pPR->m_Name)
                break;
            }
            if (m == PSParams.size())
              PSParams.push_back(pPR);
          }
        }
      }

      if (pGS)
      {
        bHasGS = true;
        for (n=0; n<pGS->m_Params.size(); n++)
        {
          SFXParam *pPR = &pGS->m_Params[n];
          if (pPR->m_bWasMerged & (bMergeMaskGS << 6))
            continue;
          if (pPR->m_nCB == CB_PER_MATERIAL)
          {
            if(pPR->m_nRegister[eHWSC_Geometry]<FIRST_REG_PM[eHWSC_Geometry] || pPR->m_nRegister[eHWSC_Geometry]>=10000)
              continue;
            for (m=0; m<GSParams.size(); m++)
            {
              if (GSParams[m]->m_Name == pPR->m_Name)
                break;
            }
            if (m == GSParams.size())
              GSParams.push_back(pPR);
          }
        }
      }

    }
  }
  gRenDev->m_RP.m_FlagsShader_LT = eHWSC_Vertex;
  if (VSParams.size() > 1)
  {
    std::sort(VSParams.begin(), VSParams.end(), CompareItem);
  }
  gRenDev->m_RP.m_FlagsShader_LT = eHWSC_Pixel;
  if (PSParams.size() > 1)
  {
    std::sort(PSParams.begin(), PSParams.end(), CompareItem);
  }
  gRenDev->m_RP.m_FlagsShader_LT = eHWSC_Geometry;
  if (GSParams.size() > 1)
  {
    std::sort(GSParams.begin(), GSParams.end(), CompareItem);
  }

  DynArray<Vec4> *pCBVS = &m_Constants[eHWSC_Vertex];
  DynArray<Vec4> *pCBPS = &m_Constants[eHWSC_Pixel];
  DynArray<Vec4> *pCBGS = &m_Constants[eHWSC_Geometry];
  SFXParam *pPR;
  int nFirstVS = FIRST_REG_PM[eHWSC_Vertex];
  int nLastVS = -1;
  int nFirstPS = FIRST_REG_PM[eHWSC_Pixel];
  int nLastPS = -1;
  int nFirstGS = FIRST_REG_PM[eHWSC_Geometry];
  int nLastGS = -1;

  if (VSParams.size())
  {
    nLastVS = VSParams[VSParams.size()-1]->m_nRegister[eHWSC_Vertex];
    assert(nLastVS>=FIRST_REG_PM[eHWSC_Vertex] && VSParams[0]->m_nRegister[eHWSC_Vertex]>=FIRST_REG_PM[eHWSC_Vertex]);
  }
  nLastVS = max(nFirstVS+NUM_VS_PM-1, nLastVS);
  int nCur = pCBVS->size();
  if (nCur < nLastVS+1-nFirstVS)
    pCBVS->resize(nLastVS+1-nFirstVS);
  for (i=nCur; i<=nLastVS-nFirstVS; i++)
  {
    (*pCBVS)[i] = Vec4(-100000,-200000,-300000,-400000);
  }

  if (PSParams.size())
  {
    nLastPS = PSParams[PSParams.size()-1]->m_nRegister[eHWSC_Pixel];
    assert(nLastPS>=nFirstPS && PSParams[0]->m_nRegister[eHWSC_Pixel]>=nFirstPS);
  }
  nLastPS = max(nFirstPS+NUM_PS_PM-1, nLastPS);
  nCur = pCBPS->size();
  if (nCur < nLastPS+1-nFirstPS)
    pCBPS->resize(nLastPS+1-nFirstPS);
  for (i=nCur; i<=nLastPS-nFirstPS; i++)
  {
    (*pCBPS)[i] = Vec4(-100000,-200000,-300000,-400000);
  }

  if (bHasGS)
  {
    if (GSParams.size())
    {
      nLastGS = GSParams[GSParams.size()-1]->m_nRegister[eHWSC_Geometry];
      assert(nLastGS>=nFirstGS && GSParams[0]->m_nRegister[eHWSC_Geometry]>=nFirstGS);
    }
    nLastGS = max(nFirstGS+NUM_GS_PM-1, nLastGS);
    nCur = pCBGS->size();
    if (nCur < nLastGS+1-nFirstGS)
      pCBGS->resize(nLastGS+1-nFirstGS);
    for (i=nCur; i<=nLastGS-nFirstGS; i++)
    {
      (*pCBGS)[i] = Vec4(-100000,-200000,-300000,-400000);
    }
  }

  if (m_ShaderParams.size())
  {
    for (i=0; i<VSParams.size(); i++)
    {
      pPR = VSParams[i];
      sSetParameter(pPR, *pCBVS, this, nFirstVS, eHWSC_Vertex);
    }
    for (i=0; i<PSParams.size(); i++)
    {
      pPR = PSParams[i];
      sSetParameter(pPR, *pCBPS, this, nFirstPS, eHWSC_Pixel);
    }

    for (i=0; i<GSParams.size(); i++)
    {
      pPR = GSParams[i];
      sSetParameter(pPR, *pCBGS, this, nFirstGS, eHWSC_Geometry);
    }
  }
#if 0
	// AlexL: the following is a memory optimization in Game mode, so that params are released
	//        but this also prevents any runtime changing during Game mode, so commented out for the moment
	//        [approved by Andrey]
  if (!gRenDev->m_bEditor)
    ReleaseParams();
#endif

#ifdef DIRECT3D10
  D3D10_BUFFER_DESC bd;
  ZeroStruct(bd);
  HRESULT hr;

  bd.Usage = D3D10_USAGE_DEFAULT;
  bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = 0;
  bd.MiscFlags = 0;

  //bd.Usage = D3D10_USAGE_DYNAMIC;
  //bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  ID3D10Buffer **ppBuf = (ID3D10Buffer **)&m_pCB[eHWSC_Vertex];
  SAFE_RELEASE(*ppBuf);
  if (m_Constants[eHWSC_Vertex].size())
  {
    bd.ByteWidth = m_Constants[eHWSC_Vertex].size() * sizeof(Vec4);
    hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, ppBuf);
    assert(SUCCEEDED(hr));
    gcpRendD3D->m_pd3dDevice->UpdateSubresource(*ppBuf, 0, NULL, &m_Constants[eHWSC_Vertex][0], 0, 0);
    //void *pData;
    //(*ppBuf)->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pData);
    //memcpy(pData, &m_Constants[eHWSC_Vertex][0], bd.ByteWidth);
    //(*ppBuf)->Unmap();
  }

  ppBuf = (ID3D10Buffer **)&m_pCB[eHWSC_Pixel];
  SAFE_RELEASE(*ppBuf);
  if (m_Constants[eHWSC_Pixel].size())
  {
    bd.ByteWidth = m_Constants[eHWSC_Pixel].size() * sizeof(Vec4);
    hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, ppBuf);
    assert(SUCCEEDED(hr));
    gcpRendD3D->m_pd3dDevice->UpdateSubresource(*ppBuf, 0, NULL, &m_Constants[eHWSC_Pixel][0], 0, 0);
    //void *pData;
    //(*ppBuf)->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pData);
    //memcpy(pData, &m_Constants[eHWSC_Pixel][0], bd.ByteWidth);
    //(*ppBuf)->Unmap();
  }

  ppBuf = (ID3D10Buffer **)&m_pCB[eHWSC_Geometry];
  SAFE_RELEASE(*ppBuf);
  if (m_Constants[eHWSC_Geometry].size())
  {
    bd.ByteWidth = m_Constants[eHWSC_Geometry].size() * sizeof(Vec4);
    hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, ppBuf);
    assert(SUCCEEDED(hr));
    gcpRendD3D->m_pd3dDevice->UpdateSubresource(*ppBuf, 0, NULL, &m_Constants[eHWSC_Geometry][0], 0, 0);
    //void *pData;
    //(*ppBuf)->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&pData);
    //memcpy(pData, &m_Constants[eHWSC_Pixel][0], bd.ByteWidth);
    //(*ppBuf)->Unmap();
  }
#endif
}

void SRenderShaderResources::CloneConstants(const IRenderShaderResources* pISrc)
{
  SRenderShaderResources *pSrc = (SRenderShaderResources *)pISrc;
	if (!pSrc)
	{
		for (int i=0; i<2; i++)
			m_Constants[i].clear();
#if defined(DIRECT3D10)
		ID3D10Buffer*& pCB0 = (ID3D10Buffer*&) m_pCB[eHWSC_Vertex];
		SAFE_RELEASE(pCB0);

		ID3D10Buffer*& pCB1 = (ID3D10Buffer*&) m_pCB[eHWSC_Pixel];
		SAFE_RELEASE(pCB1);
#endif
		return;
	}
	else
	{
		for (int i=0; i<2; i++)
			m_Constants[i] = pSrc->m_Constants[i];
#if defined(DIRECT3D10)
		{
			ID3D10Buffer*& pCB0Dst = (ID3D10Buffer*&) m_pCB[eHWSC_Vertex];
			ID3D10Buffer*& pCB0Src = (ID3D10Buffer*&) pSrc->m_pCB[eHWSC_Vertex];
			if (pCB0Src)
				pCB0Src->AddRef();
			if (pCB0Dst)
				pCB0Dst->Release();
			pCB0Dst = pCB0Src;
		}
		{
			ID3D10Buffer*& pCB1Dst = (ID3D10Buffer*&) m_pCB[eHWSC_Pixel];
			ID3D10Buffer*& pCB1Src = (ID3D10Buffer*&) pSrc->m_pCB[eHWSC_Pixel];
			if (pCB1Src)
				pCB1Src->AddRef();
			if (pCB1Dst)
				pCB1Dst->Release();
			pCB1Dst = pCB1Src;
		}
#endif
	}
}

void SRenderShaderResources::ReleaseConstants()
{
  m_Constants[0].clear();
  m_Constants[1].clear();
#if defined (DIRECT3D10)
  ID3D10Buffer **ppBuf = (ID3D10Buffer **)&m_pCB[eHWSC_Vertex];
  SAFE_RELEASE(*ppBuf);

  ppBuf = (ID3D10Buffer **)&m_pCB[eHWSC_Pixel];
  SAFE_RELEASE(*ppBuf);
#endif
}
