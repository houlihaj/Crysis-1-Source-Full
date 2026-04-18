/*=============================================================================
ShaderFXParseBin.cpp : implementation of the Shaders parser using FX language.
Copyright (c) 2001-2004 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

static FOURCC FOURCC_SHADERBIN = MAKEFOURCC('F','X','B','0');

SShaderBin SShaderBin::m_Root;
uint32 SShaderBin::m_nCache = 0;

CShaderManBin::CShaderManBin()
{
  m_pCEF = &gRenDev->m_cEF;
}

void SShaderBin::CryptData()
{
  int i;
  uint32 *pData = &m_Tokens[0];
  uint32 CRC32 = m_CRC32;
  for (i=0; i<m_Tokens.size(); i++)
  {
    pData[i] ^= CRC32;
  }
}

void SShaderBin::UpdateCRC(bool bStore)
{
  if (!m_Tokens.size())
    return;
  uint32 CRC32 = GetCRC32Gen().GetCRC32((char *)&m_Tokens[0], m_Tokens.size()*4, 0xffffffff);
  int nCur = 0;
  Lock();
  while (nCur >= 0)
  {
    nCur = CParserBin::FindToken(nCur, m_Tokens.size()-1, &m_Tokens[0], eT_include);
    if (nCur >= 0)
    {
      nCur++;
      uint32 nTokName = m_Tokens[nCur];
      const char *szNameInc = CParserBin::GetString(nTokName, m_Table);
      SShaderBin *pBinIncl = gRenDev->m_cEF.m_Bin.GetBinShader(szNameInc, true);
      assert(pBinIncl);
      if (pBinIncl)
      {
        pBinIncl->UpdateCRC(false);
        CRC32 += pBinIncl->m_CRC32;
      }
    }
  }
  Unlock();

  if (bStore && CRC32 != m_CRC32)
  {
    SShaderBinHeader Header;
    FILE *fp = iSystem->GetIPak()->FOpen(m_szName.c_str(), "rb");
    assert(fp);
    iSystem->GetIPak()->FRead((byte *)&Header, sizeof(Header), fp);
    iSystem->GetIPak()->FClose(fp);
    if (CRenderer::CV_r_shadersuserfolder && !strstr(m_szName, "%USER%"))
    {
      CCryName nm = CCryName(m_szName.c_str());
      CShaderManBin &Bin = gRenDev->m_cEF.m_Bin;
      FXShaderBinCRCItor itor = Bin.m_BinCRCs.find(nm);
      if (itor == Bin.m_BinCRCs.end())
        Bin.m_BinCRCs.insert(FXShaderBinCRCItor::value_type(nm, CRC32));
      else
        itor->second = CRC32;
    }
    else
    {
      fp = iSystem->GetIPak()->FOpen(m_szName.c_str(), "r+b");
      Header.m_CRC32 = CRC32;
      iSystem->GetIPak()->FWrite((void *)&Header, sizeof(Header), 1, fp);
      iSystem->GetIPak()->FClose(fp);

#ifndef LINUX
      char szDestBuf[256];
      const char *szDest = iSystem->GetIPak()->AdjustFileName(m_szName, szDestBuf, 0);
#if defined(PS3)
		  FILETIME ft = *((FILETIME*)&m_Time);
      SetFileTime(szDest, &ft);
#else
      HANDLE hdst = CreateFile(szDest, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
      if (hdst == INVALID_HANDLE_VALUE)
      {
        assert(0);
      }
      else
      {
        FILETIME ft = *((FILETIME*)&m_Time);
        SetFileTime(hdst, NULL, NULL, &ft);
        CloseHandle(hdst);
      }
#endif //PS3
#endif//LINUX
    }
  }
  else
  if (bStore)
  {
    CCryName nm = CCryName(m_szName.c_str());
    CShaderManBin &Bin = gRenDev->m_cEF.m_Bin;
    FXShaderBinCRCItor itor = Bin.m_BinCRCs.find(nm);
    if (itor != Bin.m_BinCRCs.end())
      itor->second = CRC32;
  }
  m_CRC32 = CRC32;
}

SShaderBin *CShaderManBin::SaveBinShader(const char *szName, bool bInclude, FILE *fpSrc)
{
  SShaderBin *pBin = new SShaderBin;

  CParserBin Parser(pBin);

  iSystem->GetIPak()->FSeek(fpSrc, 0, SEEK_END);
  int nSize = iSystem->GetIPak()->FTell(fpSrc);
  char *buf = new char [nSize+1];
  char *pBuf = buf;
  buf[nSize] = 0;
  iSystem->GetIPak()->FSeek(fpSrc, 0, SEEK_SET);
  iSystem->GetIPak()->FRead(buf, nSize, fpSrc);
  uint64 WriteTime = iSystem->GetIPak()->GetModificationTime(fpSrc);
  RemoveCR(buf);
  const char *kWhiteSpace = " ";

  while (buf[0])
  {
    SkipCharacters (&buf, kWhiteSpace);
    SkipComments (&buf, true);
    if (!buf || !buf[0])
      break;

    char com[1024];
    bool bKey = false;
    uint32 dwToken = CParserBin::NextToken(buf, com, bKey);
    dwToken = Parser.NewUserToken(dwToken, com, false);
    pBin->m_Tokens.push_back(dwToken);
    if (dwToken == eT_include)
    {
      assert(bKey);
      SkipCharacters(&buf, kWhiteSpace);
      assert(*buf == '"' || *buf == '<');
      char brak = *buf;
      ++buf;
      int n = 0;
      while(*buf != brak)
      {
        if (*buf <= 0x20)
        {
          assert(0);
          break;
        }
        com[n++] = *buf;
        ++buf;
      }
      if (*buf == brak)
        ++buf;
      com[n] = 0;

      fpStripExtension(com, com);

      SShaderBin *pBIncl = GetBinShader(com, true);
      assert(pBIncl);

      dwToken = CParserBin::fxToken(com, NULL);
      dwToken = Parser.NewUserToken(dwToken, com, false);
      pBin->m_Tokens.push_back(dwToken);
    }
    else
    if (dwToken == eT_if || dwToken == eT_ifdef || dwToken == eT_ifndef)
    {
      bool bFirst = fxIsFirstPass(buf);
      if (!bFirst)
      {
        if (dwToken == eT_if)
          dwToken = eT_if_2;
        else
        if (dwToken == eT_ifdef)
          dwToken = eT_ifdef_2;
        else
          dwToken = eT_ifndef_2;
        pBin->m_Tokens[pBin->m_Tokens.size()-1] = dwToken;
      }
    }
    else
    if (dwToken == eT_define)
    {
      shFill(&buf, com);
      if (com[0] == '%')
        pBin->m_Tokens[pBin->m_Tokens.size()-1] = eT_define_2;
      dwToken = Parser.NewUserToken(eT_unknown, com, false);
      pBin->m_Tokens.push_back(dwToken);

      TArray<char> macro;
      while (*buf == 0x20 || *buf == 0x9) {buf++;}
      while (*buf != 0xa)
      {
        if (*buf == '\\')
        {
          macro.AddElem('\n');
          while (*buf != '\n') {buf++;}
          buf++;
          continue;
        }
        macro.AddElem(*buf);
        buf++;
      }
      macro.AddElem(0);
      int n = macro.Num()-2;
      while (n >= 0 && macro[n] <= 0x20)
      {
        macro[n] = 0;
        n--;
      }
      char *b = &macro[0];
      while (*b)
      {
        SkipCharacters (&b, kWhiteSpace);
        SkipComments (&b, true);
        if (!b[0])
          break;
        bKey = false;
        dwToken = CParserBin::NextToken(b, com, bKey);
        dwToken = Parser.NewUserToken(dwToken, com, false);
        if (dwToken == eT_if || dwToken == eT_ifdef || dwToken == eT_ifndef)
        {
          bool bFirst = fxIsFirstPass(b);
          if (!bFirst)
          {
            if (dwToken == eT_if)
              dwToken = eT_if_2;
            else
            if (dwToken == eT_ifdef)
              dwToken = eT_ifdef_2;
            else
              dwToken = eT_ifndef_2;
          }
        }
        pBin->m_Tokens.push_back(dwToken);
      }
      pBin->m_Tokens.push_back(0);
    }
  }
  if (!pBin->m_Tokens[0])
    pBin->m_Tokens.push_back(eT_skip);

  pBin->UpdateCRC(false);
  //pBin->CryptData();
  //pBin->CryptTable();

  char nameFile[256];
  sprintf(nameFile, "Shaders/Cache/%s.%s", szName, bInclude ? "cfib" : "cfxb");
  string szDst = CRenderer::CV_r_shadersuserfolder ? m_pCEF->m_szUserPath + string(nameFile) : string(nameFile);
  FILE *fpDst = iSystem->GetIPak()->FOpen(szDst, "wb");
  if (fpDst)
  {
    SShaderBinHeader Header;
    Header.m_nTokens = pBin->m_Tokens.size();
    Header.m_Magic = FOURCC_SHADERBIN;
    Header.m_CRC32 = pBin->m_CRC32;
    float fVersion = (float)FX_CACHE_VER;
    Header.m_VersionLow = (uint16)(((float)fVersion - (float)(int)fVersion)*10.1f);
    Header.m_VersionHigh = (uint16)fVersion;
    Header.m_OffsetStringTable = pBin->m_Tokens.size() * sizeof(DWORD) + sizeof(Header);
    iSystem->GetIPak()->FWrite((void *)&Header, sizeof(Header), 1, fpDst);
    iSystem->GetIPak()->FWrite(&pBin->m_Tokens[0], pBin->m_Tokens.size() * sizeof(DWORD), 1, fpDst);
    FXShaderTokenItor itor;
    for (itor=pBin->m_Table.begin(); itor!=pBin->m_Table.end(); itor++)
    {
      iSystem->GetIPak()->FWrite(&itor->first, sizeof(DWORD), 1, fpDst);
      iSystem->GetIPak()->FWrite(itor->second.SToken.c_str(), itor->second.SToken.size()+1, 1, fpDst);
    }
    iSystem->GetIPak()->FClose(fpDst);
  }
  else
  {
    iLog->LogWarning("CShaderManBin::SaveBinShader: Cannot write shader to file '%s'.", nameFile);
  }

#ifndef LINUX
  char szDestBuf[256];
  const char *szDest = iSystem->GetIPak()->AdjustFileName(szDst.c_str(), szDestBuf, 0);
#if defined(PS3)
	FILETIME ft = *((FILETIME*)&WriteTime);
	SetFileTime(szDest, &ft);
#else
  HANDLE hdst = CreateFile(szDest, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (hdst == INVALID_HANDLE_VALUE)
  {
    assert(0);
  }
  else
  {
    FILETIME ft = *((FILETIME*)&WriteTime);
    SetFileTime(hdst, NULL, NULL, &ft);
    CloseHandle(hdst);
  }
#endif//PS3
#endif//LINUX

  SAFE_DELETE_ARRAY(pBuf);

  return pBin;
}

SShaderBin *CShaderManBin::LoadBinShader(FILE *fpBin, const char *szName, const char *szNameBin)
{
  iSystem->GetIPak()->FSeek(fpBin, 0, SEEK_END);
  int nSize = iSystem->GetIPak()->FTell(fpBin);
  char *buf = new char [nSize+1];
  buf[nSize] = 0;
  iSystem->GetIPak()->FSeek(fpBin, 0, SEEK_SET);
  iSystem->GetIPak()->FRead(buf, nSize, fpBin);

  uint64 dwTime = iSystem->GetIPak()->GetModificationTime(fpBin);

  SShaderBinHeader *pHeader = (SShaderBinHeader *)buf;
  float fVersion = (float)FX_CACHE_VER;
  uint16 MinorVer = (uint16)(((float)fVersion - (float)(int)fVersion)*10.1f);
  uint16 MajorVer = (uint16)fVersion;
  if (pHeader->m_VersionLow != MinorVer || pHeader->m_VersionHigh != MajorVer || pHeader->m_Magic != FOURCC_SHADERBIN)
  {
    SAFE_DELETE_ARRAY(buf);
    return NULL;
  }
  SShaderBin *pBin = new SShaderBin;
  CParserBin Parser(pBin);
  pBin->m_Time = dwTime;

  uint32 CRC32 = pHeader->m_CRC32;
  CCryName nm = CCryName(szNameBin);
  CShaderManBin &Bin = gRenDev->m_cEF.m_Bin;
  FXShaderBinCRCItor itor = Bin.m_BinCRCs.find(nm);
  if (itor != Bin.m_BinCRCs.end())
    CRC32 = itor->second;

  pBin->m_CRC32 = CRC32;
  int nSizeTable = nSize - pHeader->m_OffsetStringTable;
  pBin->m_Tokens.resize(pHeader->m_nTokens);
  memcpy(&pBin->m_Tokens[0], &buf[sizeof(SShaderBinHeader)], pHeader->m_nTokens*sizeof(uint32));
  //pBin->CryptData();
  char *bufTable = &buf[pHeader->m_OffsetStringTable];
  while (bufTable < &buf[nSize])
  {
    uint32 nToken = *(uint32 *)bufTable;
    FXShaderTokenItor it = pBin->m_Table.find(nToken);
    if (it != pBin->m_Table.end())
    {
      assert(0);
    }
    else
    {
      STokenD TD;
      TD.SToken = &bufTable[4];
      pBin->m_Table.insert(FXShaderTokenItor::value_type(nToken, TD));
    }
    int nIncr = 4 + strlen(&bufTable[4]) + 1;
    bufTable += nIncr;
  }
  char nameLwr[256];
  strcpy(nameLwr, szName);
  strlwr(nameLwr);
  pBin->m_szName = szNameBin;
  pBin->m_dwName = CParserBin::GetCRC32(nameLwr);
  SAFE_DELETE_ARRAY(buf);

  return pBin;
}

SShaderBin *CShaderManBin::SearchInCache(const char *szName, bool bInclude)
{
  char nameFile[256], nameLwr[256];
  strcpy(nameLwr, szName);
  strlwr(nameLwr);
  sprintf(nameFile, "%s.%s", nameLwr, bInclude ? "cfi" : "cfx");
  uint32 dwName = CParserBin::GetCRC32(nameFile);

  SShaderBin *pSB;
  for (pSB=SShaderBin::m_Root.m_Prev; pSB!=&SShaderBin::m_Root; pSB=pSB->m_Prev)
  {
    if (pSB->m_dwName == dwName)
    {
      pSB->Unlink();
      pSB->Link(&SShaderBin::m_Root);
      return pSB;
    }
  }
  return NULL;
}

bool CShaderManBin::AddToCache(SShaderBin *pSB)
{
  if (SShaderBin::m_nCache >= MAX_FXBIN_CACHE)
  {
    SShaderBin *pS;
    for (pS=SShaderBin::m_Root.m_Prev; pS!=&SShaderBin::m_Root; pS=pS->m_Prev)
    {
      if (!pS->m_bLocked)
      {
        DeleteFromCache(pS);
        break;
      }
    }
  }
  assert(SShaderBin::m_nCache < MAX_FXBIN_CACHE);

  pSB->Link(&SShaderBin::m_Root);
  SShaderBin::m_nCache++;

  return true;
}

bool CShaderManBin::DeleteFromCache(SShaderBin *pSB)
{
  assert(pSB != &SShaderBin::m_Root);
  pSB->Unlink();
  SAFE_DELETE(pSB);
  SShaderBin::m_nCache--;

  return true;
}

void CShaderManBin::InvalidateCache()
{
  SShaderBin *pSB, *Next;
  for (pSB=SShaderBin::m_Root.m_Next; pSB!=&SShaderBin::m_Root; pSB=Next)
  {
    Next = pSB->m_Next;
    DeleteFromCache(pSB);
  }
}

SShaderBin *CShaderManBin::GetBinShader(const char *szName, bool bInclude, bool *pbChanged)
{
	//for PS3 loading time matters, by using a special flag the sources do not matter and no modification time check is performed
  if (pbChanged)
    *pbChanged = true;

  SShaderBin *pSHB = SearchInCache(szName, bInclude);
  if (pSHB)
    return pSHB;
  SShaderBinHeader Header[2];
  char nameFile[256], nameBin[256];
	FILE *fpSrc;
	uint64 WriteTime;
#if defined(PS3)
	if(!gPS3Env->bDisableCgc)
	{
#endif
  sprintf(nameFile, "%sCryFX/%s.%s", gRenDev->m_cEF.m_ShadersPath, szName, bInclude ? "cfi" : "cfx");
  fpSrc = iSystem->GetIPak()->FOpen(nameFile, "rb");
  WriteTime = fpSrc ? iSystem->GetIPak()->GetModificationTime(fpSrc) : 0;
  //char szPath[1024];
  //getcwd(szPath, 1024);
#if defined(PS3)
	}
#endif
  sprintf(nameBin, "Shaders/Cache/%s.%s", szName, bInclude ? "cfib" : "cfxb");
  FILE *fpDst = NULL;
  int i=0, n = 1;
  string szDst;
#if defined(PS3)
	if(!gPS3Env->bDisableCgc)
	{
#endif
  if (CRenderer::CV_r_shadersuserfolder)
  {
    szDst = m_pCEF->m_szUserPath + string(nameBin);
    n = 2;
  }
  else
  {
    szDst = string(nameBin);
    n = 1;
  }
  byte bValid = 0;
  float fVersion = (float)FX_CACHE_VER;
  for (i=0; i<n; i++)
  {
    if (!i)
      fpDst = iSystem->GetIPak()->FOpen(nameBin, "rb");
    else
      fpDst = iSystem->GetIPak()->FOpen(szDst.c_str(), "rb");
    if (!fpDst)
      continue;
    else
    {
      uint64 DstWriteTime = fpSrc ? iSystem->GetIPak()->GetModificationTime(fpDst) : 0;
      if (DstWriteTime != WriteTime)
        bValid |= 1<<i;
      else
      {
				//if changing these lines, please also do below where dedicated PS3 stuff is
        iSystem->GetIPak()->FRead((byte *)&Header[i], sizeof(SShaderBinHeader), fpDst);
        uint16 MinorVer = (uint16)(((float)fVersion - (float)(int)fVersion)*10.1f);
        uint16 MajorVer = (uint16)fVersion;
        if (Header[i].m_VersionLow != MinorVer || Header[i].m_VersionHigh != MajorVer || Header[i].m_Magic != FOURCC_SHADERBIN)
          bValid |= 4<<i;
      }
    }
    if (!(bValid & (5<<i)))
      break;
  }
  if (i==n)
  {
    if (bValid & 1)
      CryLog("WARNING: Bin FXShader '%s' time mismatch", nameBin);
    if (bValid & 4)
      LogWarning("WARNING: Bin FXShader '%s' version mismatch (Cache: %d.%d, Current: %.1f)", nameBin, Header[0].m_VersionHigh, Header[0].m_VersionLow, fVersion);

    if (bValid & 2)
      CryLog("WARNING: Bin FXShader '%s' time mismatch", szDst.c_str());
    if (bValid & 8)
      LogWarning("WARNING: Bin FXShader '%s' version mismatch (Cache: %d.%d, Current: %.1f)", szDst.c_str(), Header[1].m_VersionHigh, Header[1].m_VersionLow, fVersion);

    if (fpDst)
    {
      iSystem->GetIPak()->FClose(fpDst);
      fpDst = NULL;
    }
    i = 1;

    if (fpSrc)
    {
      pSHB = SaveBinShader(szName, bInclude, fpSrc);
      assert(!pSHB->m_Next);
      SAFE_DELETE(pSHB);
      fpDst = iSystem->GetIPak()->FOpen(szDst.c_str(), "rb");
      if (pbChanged)
        *pbChanged = true;
      iSystem->GetIPak()->FClose(fpSrc);
      fpSrc = NULL;
    }
  }
#if defined(PS3)
	}
	else
	{
		//do only perform the necessary stuff
		fpDst = iSystem->GetIPak()->FOpen(nameBin, "rb");
		if (fpDst)
			iSystem->GetIPak()->FRead((byte *)&Header[i], sizeof(SShaderBinHeader), fpDst);
	}
#endif
  if (fpSrc)
    iSystem->GetIPak()->FClose(fpSrc);
  if (fpDst)
  {
    sprintf(nameFile, "%s.%s", szName, bInclude ? "cfi" : "cfx");
    pSHB = LoadBinShader(fpDst, nameFile, i==0 ? nameBin : szDst.c_str());
    iSystem->GetIPak()->FClose(fpDst);
    assert(pSHB);
    if (pSHB)
    {
      AddToCache(pSHB);
      if (!bInclude)
      {
        FXShaderBinPathItor it = m_BinPaths.find(CCryName(nameFile));
        if (it == m_BinPaths.end())
          m_BinPaths.insert(FXShaderBinPath::value_type(CCryName(nameFile), i==0 ? string(nameBin) : szDst));
      }
    }
  }
  if (!pSHB)
  {
    if (fpDst)
      Warning("Error: Failed to get binary shader '%s'", nameFile);
    else
      Warning("Error: Shader '%s' doesn't exist", nameFile);
  }

  return pSHB;
}

void CShaderManBin::AddGenMacroses(SShaderGen *shG, CParserBin& Parser, uint nMaskGen)
{
  if (!nMaskGen || !shG)
    return;

  uint32 dwMacro = eT_1;
  for (int i=0; i<shG->m_BitMask.Num(); i++)
  {
    if (shG->m_BitMask[i]->m_Mask & nMaskGen)
    {
      Parser.AddMacro(shG->m_BitMask[i]->m_dwToken, &dwMacro, 1, shG->m_BitMask[i]->m_Mask, Parser.m_Macros[1]);
    }
  }
}

bool CShaderManBin::ParseBinFX_Global_Annotations(CParserBin& Parser, SParserFrame& Frame, bool *bPublic, CCryName techStart[2])
{
  bool bRes = true;

  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(ShaderType)
    FX_TOKEN(ShaderDrawType)
    FX_TOKEN(PreprType)
    FX_TOKEN(Public)
    FX_TOKEN(StartTecnique)
    FX_TOKEN(NoPreview)
    FX_TOKEN(LocalConstants)
    FX_TOKEN(Cull)
    FX_TOKEN(SupportsAttrInstancing)
    FX_TOKEN(SupportsConstInstancing)
    FX_TOKEN(Decal)
    FX_TOKEN(DecalNoDepthOffset)
    FX_TOKEN(DetailBumpMapping)
    FX_TOKEN(EnvLighting)
    FX_TOKEN(NoChunkMerging)
    FX_TOKEN(ForceTransPass)
    FX_TOKEN(AfterHDRPostProcess)
    FX_TOKEN(ForceZpass)
    FX_TOKEN(ForceWaterPass)
    FX_TOKEN(ForceDrawLast)
    FX_TOKEN(Hair)
    FX_TOKEN(ForceGeneralPass)
    FX_TOKEN(ForceDrawAfterWater)
    FX_TOKEN(SupportsSubSurfaceScattering)
    FX_TOKEN(SupportsReplaceBasePass)
    FX_TOKEN(SingleLightPass)
    FX_TOKEN(SkipGeneralPass)
    FX_TOKEN(Refractive)
    FX_TOKEN(VT_DetailBending)
    FX_TOKEN(VT_DetailBendingGrass)
    FX_TOKEN(VT_TerrainAdapt)
    FX_TOKEN(VT_WindBending)
  FX_END_TOKENS

  int nIndex;
  CShader *ef = Parser.m_pCurShader;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_Public:
        if (bPublic)
          *bPublic = true;
        break;

      case eT_NoPreview:
        if (!ef)
          break;
        ef->m_Flags |= EF_NOPREVIEW;
        break;

      case eT_Decal:
        if (!ef)
          break;
        ef->m_Flags |= EF_DECAL;
        ef->m_nMDV |= MDV_DEPTH_OFFSET;
        break;

      case eT_DecalNoDepthOffset:
        if (!ef)
          break;
        ef->m_Flags |= EF_DECAL;
        break;


      case eT_LocalConstants:
        if (!ef)
          break;
        ef->m_Flags |= EF_LOCALCONSTANTS;
        break;

      case eT_VT_DetailBending:
        if (!ef)
          break;
        ef->m_nMDV |= MDV_DET_BENDING;
        break;
      case eT_VT_DetailBendingGrass:
        if (!ef)
          break;
        ef->m_nMDV |= MDV_DET_BENDING | MDV_DET_BENDING_GRASS;
        break;
      case eT_VT_WindBending:
        if (!ef)
          break;
        ef->m_nMDV |= MDV_WIND;
        break;
      case eT_VT_TerrainAdapt:
        if (!ef)
          break;
        ef->m_nMDV |= MDV_TERRAIN_ADAPT;
        break;

      case eT_EnvLighting:
        if (!ef)
          break;
        ef->m_Flags |= EF_ENVLIGHTING;
        break;

      case eT_NoChunkMerging:
        if (!ef)
          break;
        ef->m_Flags |= EF_NOCHUNKMERGING;
        break;

      case eT_SupportsAttrInstancing:
        if (!ef)
          break;
        if (gRenDev->m_bDeviceSupportsInstancing)
          ef->m_Flags |= EF_SUPPORTSINSTANCING_ATTR;
        break;
      case eT_SupportsConstInstancing:
        if (!ef)
          break;
        if (gRenDev->m_bDeviceSupportsInstancing)
          ef->m_Flags |= EF_SUPPORTSINSTANCING_CONST;
        break;

      case eT_ForceTransPass:        
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_TRANSPASS;
        break;

      case eT_AfterHDRPostProcess: 
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_AFTERHDRPOSTPROCESS;
        break;

      case eT_ForceZpass:        
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_ZPASS;
        break;

      case eT_ForceWaterPass:        
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_WATERPASS;
        break;

      case eT_ForceDrawLast:        
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_DRAWLAST;
        break;
      case eT_Hair:        
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_HAIR;
        break;

      case eT_ForceGeneralPass: 
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_GENERALPASS;
        break;

      case eT_ForceDrawAfterWater:
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_FORCE_DRAWAFTERWATER;
        break;
      case eT_SupportsSubSurfaceScattering:
        if (!ef)
          break;
				eT = Parser.GetToken(Parser.m_Data);
				if (eT == eT_ScatterBlend)
				{
					ef->m_Flags2 |= EF2_SUBSURFSCATTER;
					ef->m_Flags2 |= EF2_BLENDSUBSURFSCATTER;
				}
				else if (eT == eT_NoScatterBlend)
				{
					ef->m_Flags2 |= EF2_SUBSURFSCATTER;
					ef->m_Flags2 &= ~EF2_BLENDSUBSURFSCATTER;
				}
				else
				{
					Warning("Unknown subsurface scattering mode '%s'", Parser.GetString(eT));
					assert(0);
				}
        break;
      case eT_SupportsReplaceBasePass:
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_SUPPORTS_REPLACEBASEPASS;
        break;
      case eT_SingleLightPass:
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_SINGLELIGHTPASS;      
        break;
      case eT_SkipGeneralPass:
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_SKIPGENERALPASS;      
        break;
      case eT_Refractive:
        if (!ef)
          break;
        ef->m_Flags |= EF_REFRACTIVE;
        break;

      case eT_DetailBumpMapping:
        if (!ef)
          break;
        ef->m_Flags2 |= EF2_DETAILBUMPMAPPING;
        break;

      case eT_ShaderDrawType:
        {
          if (!ef)
            break;
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_Light)
            ef->m_eSHDType = eSHDT_Light;
          else
          if (eT == eT_Shadow)
            ef->m_eSHDType = eSHDT_Shadow;
          else
          if (eT == eT_Fur)
            ef->m_eSHDType = eSHDT_Fur;
          else
          if (eT == eT_General)
            ef->m_eSHDType = eSHDT_General;
          else
          if (eT == eT_Terrain)
            ef->m_eSHDType = eSHDT_Terrain;
          else
          if (eT == eT_Overlay)
            ef->m_eSHDType = eSHDT_Overlay;
          else
          if (eT == eT_NoDraw)
          {
            ef->m_eSHDType = eSHDT_NoDraw;
            ef->m_Flags2 |= EF2_NODRAW;
          }
          else
          if (eT == eT_Custom)
            ef->m_eSHDType = eSHDT_CustomDraw;
          else
          if (eT == eT_Sky)
          {
            ef->m_eSHDType = eSHDT_Sky;
            ef->m_Flags |= EF_SKY;
          }
          else
          if (eT == eT_OceanShore)
            ef->m_eSHDType = eSHDT_OceanShore;
          else
          {
            Warning("Unknown shader draw type '%s'", Parser.GetString(eT));
            assert(0);
          }
        }
        break;

      case eT_ShaderType:
        {
          if (!ef)
            break;
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_General)
            ef->m_eShaderType = eST_General;
          else
          if (eT == eT_Metal)
            ef->m_eShaderType = eST_Metal;
          else
          if (eT == eT_Ice)
            ef->m_eShaderType = eST_Ice;
          else
          if (eT == eT_Shadow)
            ef->m_eShaderType = eST_Shadow;
          else
          if (eT == eT_Water)
            ef->m_eShaderType = eST_Water;
          else
          if (eT == eT_FX)
            ef->m_eShaderType = eST_FX;
          else
          if (eT == eT_PostProcess)
            ef->m_eShaderType = eST_PostProcess;
          else
          if (eT == eT_HDR)
            ef->m_eShaderType = eST_HDR;
          else
          if (eT == eT_Sky)
            ef->m_eShaderType = eST_Sky;
          else
          if (eT == eT_Glass)
            ef->m_eShaderType = eST_Glass;
          else
          if (eT == eT_Vegetation)
            ef->m_eShaderType = eST_Vegetation;
          else
          if (eT == eT_Particle)
            ef->m_eShaderType = eST_Particle;
          else
          if (eT == eT_Terrain)
            ef->m_eShaderType = eST_Terrain;
          else
          {
            Warning("Unknown shader type '%s'", Parser.GetString(eT));
            assert(0);
          }
        }
        break;

      case eT_PreprType:
        {
          if (!ef)
            break;
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_GenerateSprites)
            ef->m_Flags2 |= EF2_PREPR_GENSPRITES;
          else
          {
            Warning("Unknown preprocess type '%s'", Parser.GetString(eT));
            assert(0);
          }
        }
        break;

      case eT_Cull:
        {
          if (!ef)
            break;
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_None || eT == eT_NONE)
            ef->m_eCull = eCULL_None;
          else
          if (eT == eT_CCW || eT == eT_Back)
            ef->m_eCull = eCULL_Back;
          else
          if (eT == eT_CW || eT == eT_Front)
            ef->m_eCull = eCULL_Front;
          else
            assert(0);
        }
        break;

      case eT_StartTecnique:
        {
          if (!ef)
            break;
          uint32 *pTokens = &Parser.m_Tokens[0];
          assert(!Parser.m_Data.IsEmpty());
          uint32 nCur = Parser.m_Data.m_nFirstToken;
          uint32 nTok = pTokens[nCur++];
          const char *szName = Parser.GetString(nTok);
          ICVar *var = iConsole->GetCVar(szName);
          if (!var)
            Warning("Couldn't find console variable '%s'", szName);
          else
          {
            if (!ef->m_pSelectTech)
              ef->m_pSelectTech = new STechniqueSelector;
            nTok = pTokens[nCur++];
            if (nTok == eT_question)
            {
              ef->m_pSelectTech->m_pCVarTech = var;
              ef->m_pSelectTech->m_eCompTech = eCF_NotEqual;
              ef->m_pSelectTech->m_fValRefTech = 0;
              szName = Parser.GetString(pTokens[nCur]);
              techStart[0] = szName;
              assert(pTokens[nCur+1] == eT_colon);
              if (pTokens[nCur+1] == eT_colon)
              {
                szName = Parser.GetString(pTokens[nCur+2]);
                techStart[1] = szName;
              }
            }
            else
            {
              assert(0);
            }
          }
        }
        break;

      default:
        assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Global(CParserBin& Parser, SParserFrame& Frame, bool* bPublic, CCryName techStart[2])
{
  bool bRes = true;

  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(string)
  FX_END_TOKENS

  int nIndex;
  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
    case eT_string:
      {
        eT = Parser.GetToken(Parser.m_Name);
        assert(eT == eT_Script);
        bRes &= ParseBinFX_Global_Annotations(Parser, Parser.m_Data, bPublic, techStart);
      }
      break;

    default:
      assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

static int sGetTAddress(uint32 nToken)
{
  switch (nToken)
  {
    case eT_Clamp:
      return TADDR_CLAMP;
    case eT_Border:
      return TADDR_BORDER;
    case eT_Wrap:
      return TADDR_WRAP;
    case eT_Mirror:
      return TADDR_MIRROR;
    default:
      assert(0);
  }
  return -1;
}

bool CShaderManBin::ParseBinFX_Sampler_Annotations_Script(CParserBin& Parser, SParserFrame& Frame, STexSampler *pSampler)
{
  bool bRes = true;

  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(RenderOrder)
    FX_TOKEN(ProcessOrder)
    FX_TOKEN(RenderCamera)
    FX_TOKEN(RenderType)
    FX_TOKEN(RenderFilter)
    FX_TOKEN(RenderColorTarget1)
    FX_TOKEN(RenderDepthStencilTarget)
    FX_TOKEN(ClearSetColor)
    FX_TOKEN(ClearSetDepth)
    FX_TOKEN(ClearTarget)
    FX_TOKEN(RenderTarget_IDPool)
    FX_TOKEN(RenderTarget_UpdateType)
    FX_TOKEN(RenderTarget_Width)
    FX_TOKEN(RenderTarget_Height)
    FX_TOKEN(GenerateMips)
  FX_END_TOKENS

  SHRenderTarget *pRt = new SHRenderTarget;
  int nIndex;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_RenderOrder:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_PreProcess)
            pRt->m_eOrder = eRO_PreProcess;
          else
          if (eT == eT_PostProcess)
            pRt->m_eOrder = eRO_PostProcess;
          else
          if (eT == eT_PreDraw)
            pRt->m_eOrder = eRO_PreDraw;
          else
          {
            assert(0);
            Warning("Unknown RenderOrder type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_ProcessOrder:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_WaterReflection)
            pRt->m_nProcessFlags = FSPR_SCANTEXWATER;
          else
          if (eT == eT_Panorama)
            pRt->m_nProcessFlags = FSPR_PANORAMA;
          else
          {
            assert(0);
            Warning("Unknown ProcessOrder type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_RenderCamera:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_WaterPlaneReflected)
            pRt->m_nFlags |= FRT_CAMERA_REFLECTED_WATERPLANE;
          else
          if (eT == eT_PlaneReflected)
            pRt->m_nFlags |= FRT_CAMERA_REFLECTED_PLANE;
          else
          if (eT == eT_Current)
            pRt->m_nFlags |= FRT_CAMERA_CURRENT;
          else
          {
            assert(0);
            Warning("Unknown RenderCamera type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_GenerateMips:
        pRt->m_nFlags |= FRT_GENERATE_MIPS;
        break;

      case eT_RenderType:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_CurObject)
            pRt->m_nFlags |= FRT_RENDTYPE_CUROBJECT;
          else
          if (eT == eT_CurScene)
            pRt->m_nFlags |= FRT_RENDTYPE_CURSCENE;
          else
          if (eT == eT_RecursiveScene)
            pRt->m_nFlags |= FRT_RENDTYPE_RECURSIVECURSCENE;
          else
          if (eT == eT_CopyScene)
            pRt->m_nFlags |= FRT_RENDTYPE_COPYSCENE;
          else
          {
            assert(0);
            Warning("Unknown RenderType type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_RenderFilter:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_Refractive)
            pRt->m_nFilterFlags |= FRF_REFRACTIVE;
          else
          if (eT == eT_Glow)
            pRt->m_nFilterFlags |= FRF_GLOW;
          else
          if (eT == eT_Heat)
            pRt->m_nFilterFlags |= FRF_HEAT;
          else
          {
            assert(0);
            Warning("Unknown RenderFilter type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_RenderDepthStencilTarget:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_DepthBuffer || eT == eT_DepthBufferTemp)
            pRt->m_bTempDepth = true;
          else
          if (eT == eT_DepthBufferOrig)
            pRt->m_bTempDepth = false;
          else
          {
            assert(0);
            Warning("Unknown RenderDepthStencilTarget type '%s'", Parser.GetString(eT));
          }
        }
        break;

      case eT_RenderTarget_IDPool:
        pRt->m_nIDInPool = Parser.GetInt(Parser.GetToken(Parser.m_Data));
        assert(pRt->m_nIDInPool >= 0 && pRt->m_nIDInPool < 64);
        break;

      case eT_RenderTarget_Width:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_$ScreenSize)
            pRt->m_nWidth = -1;
          else
            pRt->m_nWidth = Parser.GetInt(eT);
        }
        break;

      case eT_RenderTarget_Height:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_$ScreenSize)
            pRt->m_nHeight = -1;
          else
            pRt->m_nHeight = Parser.GetInt(eT);
        }
        break;

      case eT_RenderTarget_UpdateType:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_WaterReflect)
            pRt->m_eUpdateType = eRTUpdate_WaterReflect;
          else
          if (eT == eT_Allways)
            pRt->m_eUpdateType = eRTUpdate_Always;
          else
            assert(0);
        }
        break;

      case eT_ClearSetColor:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_FogColor)
            pRt->m_nFlags |= FRT_CLEAR_FOGCOLOR;
          else
          {
            string sStr = Parser.GetString(Parser.m_Data);
            shGetColor(sStr.c_str(), pRt->m_ClearColor);
          }
        }
        break;

      case eT_ClearSetDepth:
        pRt->m_fClearDepth = Parser.GetFloat(Parser.m_Data);
        break;

      case eT_ClearTarget:
        {
          eT = Parser.GetToken(Parser.m_Data);
          if (eT == eT_Color)
            pRt->m_nFlags |= FRT_CLEAR_COLOR;
          else
          if (eT == eT_Depth)
            pRt->m_nFlags |= FRT_CLEAR_DEPTH;
          else
          {
            assert(0);
            Warning("Unknown ClearTarget type '%s'", Parser.GetString(eT));
          }
        }
        break;

      default:
        assert(0);
    }
  }

  if (!strnicmp(pSampler->m_Texture.c_str(), "$RT_2D", 6))
  {
    if (pRt->m_nIDInPool >= 0)
    {
      if ((int)CTexture::m_CustomRT_2D.Num() <= pRt->m_nIDInPool)
        CTexture::m_CustomRT_2D.Expand(pRt->m_nIDInPool+1);
    }
    pRt->m_pTarget[0] = CTexture::m_Text_RT_2D;
  }
  else
  if (!strnicmp(pSampler->m_Texture.c_str(), "$RT_Cube", 8) || !strnicmp(pSampler->m_Name.c_str(), "$RT_CM", 6))
  {
    if (pRt->m_nIDInPool >= 0)
    {
      if ((int)CTexture::m_CustomRT_CM.Num() <= pRt->m_nIDInPool)
        CTexture::m_CustomRT_CM.Expand(pRt->m_nIDInPool+1);
    }
    pRt->m_pTarget[0] = CTexture::m_Text_RT_CM;
  }
  pSampler->m_pTarget = pRt;

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Sampler_Annotations(CParserBin& Parser, SParserFrame& Annotations, STexSampler *pSampler)
{
  bool bRes = true;

  SParserFrame OldFrame = Parser.BeginFrame(Annotations);

  FX_BEGIN_TOKENS
    FX_TOKEN(string)
  FX_END_TOKENS

  int nIndex = 0;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    SCodeFragment Fr;
    switch (eT)
    {
      case eT_string:
        {
          eT = Parser.GetToken(Parser.m_Name);
          assert(eT == eT_Script);
          bRes &= ParseBinFX_Sampler_Annotations_Script(Parser, Parser.m_Data, pSampler);
        }
        break;

      default:
        assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Sampler(CParserBin& Parser, SParserFrame& Frame, uint32 dwName, SParserFrame Annotations)
{
  bool bRes = true;

  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(Texture)
    FX_TOKEN(MinFilter)
    FX_TOKEN(MagFilter)
    FX_TOKEN(MipFilter)
    FX_TOKEN(AddressU)
    FX_TOKEN(AddressV)
    FX_TOKEN(AddressW)
    FX_TOKEN(BorderColor)
    FX_TOKEN(AnisotropyLevel)
    FX_TOKEN(sRGBLookup)
  FX_END_TOKENS

  STexSampler samp;
  STexState ST;
  DWORD dwBorderColor = 0;
  uint32 nFiltMin = 0;
  uint32 nFiltMip = 0;
  uint32 nFiltMag = 0;
  uint32 nAddressU = 0;
  uint32 nAddressV = 0;
  uint32 nAddressW = 0;
  uint32 nAnisotropyLevel = 0;

  int nIndex = -1;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_Texture:
        samp.m_Texture = Parser.GetString(Parser.m_Data);
        break;

      case eT_BorderColor:
        {
          string Str = Parser.GetString(Parser.m_Data);
          ColorF colBorder = Col_Black;
          shGetColor(Str.c_str(), colBorder);
          dwBorderColor = colBorder.pack_argb8888();
          ST.m_bActive = true;
        }
        break;

      case eT_sRGBLookup:
        ST.m_bSRGBLookup = Parser.GetBool(Parser.m_Data);
        break;

      case eT_AnisotropyLevel:
        nAnisotropyLevel = Parser.GetToken(Parser.m_Data);
        break;

      case eT_MinFilter:
        nFiltMin = Parser.GetToken(Parser.m_Data);
        ST.m_bActive = true;
        break;
      case eT_MagFilter:
        nFiltMag = Parser.GetToken(Parser.m_Data);
        ST.m_bActive = true;
        break;
      case eT_MipFilter:
        nFiltMip = Parser.GetToken(Parser.m_Data);
        ST.m_bActive = true;
        break;
      case eT_AddressU:
        nAddressU = sGetTAddress(Parser.GetToken(Parser.m_Data));
        ST.m_bActive = true;
        break;
      case eT_AddressV:
        nAddressV = sGetTAddress(Parser.GetToken(Parser.m_Data));
        ST.m_bActive = true;
        break;
      case eT_AddressW:
        nAddressW = sGetTAddress(Parser.GetToken(Parser.m_Data));
        ST.m_bActive = true;
        break;

    default:
      assert(0);
    }
  }

  samp.m_Name = Parser.GetString(dwName);
  if (nFiltMag > 0 && nFiltMin > 0 && nFiltMip > 0)
  {
    if (nFiltMag == eT_LINEAR && nFiltMin == eT_LINEAR && nFiltMip == eT_LINEAR)
      ST.SetFilterMode(FILTER_TRILINEAR);
    else
    if (nFiltMag == eT_LINEAR && nFiltMin == eT_LINEAR && nFiltMip == eT_POINT)
      ST.SetFilterMode(FILTER_BILINEAR);
    else
    if (nFiltMag == eT_LINEAR && nFiltMin == eT_LINEAR && nFiltMip == eT_NONE)
      ST.SetFilterMode(FILTER_LINEAR);
    else
    if (nFiltMag == eT_POINT && nFiltMin == eT_POINT)
      ST.SetFilterMode(FILTER_POINT);
    else
    if (nFiltMag == eT_ANISOTROPIC || nFiltMin == eT_ANISOTROPIC)
    {
      if (nAnisotropyLevel == eT_4)
        ST.SetFilterMode(FILTER_ANISO4X);
      else
      if (nAnisotropyLevel == eT_8)
        ST.SetFilterMode(FILTER_ANISO8X);
      else
      if (nAnisotropyLevel == eT_16)
        ST.SetFilterMode(FILTER_ANISO16X);
      else
        ST.SetFilterMode(FILTER_ANISO2X);
    }
    else
    {
      Warning("Unknown sampler filter mode (Min=%s, Mag=%s, Mip=%s) for sampler '%s'", Parser.GetString(nFiltMin), Parser.GetString(nFiltMag), Parser.GetString(nFiltMip), samp.m_Name.c_str());
      assert(0);
    }
  }
  else
    ST.SetFilterMode(-1);

  ST.SetClampMode(nAddressU, nAddressV, nAddressW);
  ST.SetBorderColor(dwBorderColor);
  samp.m_nTexState = CTexture::GetTexState(ST);
  if (!Annotations.IsEmpty())
    bRes &= ParseBinFX_Sampler_Annotations(Parser, Annotations, &samp);
  Parser.m_Samplers.push_back(samp);

  Parser.EndFrame(OldFrame);

  return bRes;
}

char *CShaderManBin::AddAffectedOperand(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, char *szE, int& nCB)
{
  char temp[256];
  SkipCharacters(&szE, kWhiteSpace);
  if (*szE == '(')
    szE = AddAffectedParametersForExpression(Parser, AffectedParams, AffectedFunc, szE, nCB);
  else
  {
    fxFillParam(&szE, temp);
    if (temp[0] != '.' && (temp[0]<'0' || temp[0]>'9'))
    {
      SFXParam *pr = gRenDev->m_cEF.mfGetFXParameter(Parser.m_Parameters, temp);
      if (!pr)
      {
        assert(0);
      }
      else
      {
        nCB = pr->m_nCB;
        if (!pr->m_bAffected)
        {
          pr->m_bAffected = true;
          AffectedParams.push_back(*pr);
          nCB = pr->m_nCB;
          if (pr->m_Assign.empty() && pr->m_Values[0] == '(')
            AddAffectedParametersForExpression(Parser, AffectedParams, AffectedFunc, pr->m_Values.c_str(), nCB);
        }
      }
    }
  }
  return szE;
}

char *CShaderManBin::AddAffectedParametersForExpression(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, const char *szExpr, int& nCB)
{
  assert(szExpr[0] == '(');
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
  szE = AddAffectedOperand(Parser, AffectedParams, AffectedFunc, szE, nCB);
  SkipCharacters(&szE, kWhiteSpace);
  szE++;
  SkipCharacters(&szE, kWhiteSpace);
  szE = AddAffectedOperand(Parser, AffectedParams, AffectedFunc, szE, nCB);
  return (char *)szExpr;
}

void CShaderManBin::AddAffectedParameter(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, SFXParam *pParam, EHWShaderClass eSHClass, uint32 dwType, SShaderTechnique *pShTech)
{
  static uint32 eT_ShadowGen[] = {CParserBin::GetCRC32("ShadowGenVS"), CParserBin::GetCRC32("ShadowGenPS"), CParserBin::GetCRC32("ShadowGenGS")};
  if (!CParserBin::m_bD3D10)
  {
    //if (pParam->m_Name.c_str()[0] != '_')
    {
      if (!(Parser.m_pCurShader->m_Flags & EF_LOCALCONSTANTS))
      {
        if (pParam->m_nRegister[eSHClass]>=0 && pParam->m_nRegister[eSHClass]<1000)
        {
          if (pParam->m_nCB == CB_PER_SHADOWGEN && dwType != eT_ShadowGen[eSHClass])
            return;
          if ((pShTech->m_Flags & FHF_NOLIGHTS) && pParam->m_nCB == CB_PER_LIGHT)
            return;
          if ((pShTech->m_Flags & FHF_NOLIGHTS) && pParam->m_nCB == CB_PER_MATERIAL && !(pParam->m_nFlags & PF_TWEAKABLE_MASK))
            return;
          if (pParam->m_Assign.empty() && pParam->m_Values[0] == '(')
            pParam->m_nCB = CB_PER_MATERIAL;
          AffectedParams.push_back(*pParam);
          return;
        }
      }
    }
  }
  else
  if (pParam->m_nCB == CB_PER_LIGHT || pParam->m_nCB == CB_PER_MATERIAL || pParam->m_nCB == CB_PER_FRAME || pParam->m_nCB == CB_PER_SHADOWGEN)
  {
    if (pParam->m_nRegister[eSHClass]<0 || pParam->m_nRegister[eSHClass]>=1000)
      return;
  }
  int nFlags = pParam->GetParamFlags();
  bool bCheckAffect = true;
  if (nFlags & PF_ALWAYS)
    AffectedParams.push_back(*pParam);
  else
  {
    if (CParserBin::m_bD3D10)
    {
      if (((nFlags & PF_TWEAKABLE_MASK) || pParam->m_Values[0] == '(') && pParam->m_nRegister[eSHClass]>=0 && pParam->m_nRegister[eSHClass]<1000)
        bCheckAffect = false;
    }
    for (int j=0; j<AffectedFunc.size(); j++)
    {
      SCodeFragment *pCF = &Parser.m_CodeFragments[AffectedFunc[j]];
      if (!bCheckAffect || Parser.FindToken(pCF->m_nFirstToken, pCF->m_nLastToken, pParam->m_dwName[0]) >= 0)
      {
        if (pParam->m_Assign.empty() && pParam->m_Values[0] == '(')
        {
          int nCB = CB_PER_MATERIAL;
          AddAffectedParametersForExpression(Parser, AffectedParams, AffectedFunc, pParam->m_Values.c_str(), nCB);
          pParam->m_nCB = nCB;
        }
        pParam->m_bAffected = true;
        AffectedParams.push_back(*pParam);
        break;
      }
    }
  }
}


static int gFind;
void CShaderManBin::CheckFunctionsAffect_r(CParserBin& Parser, SCodeFragment *pFunc, DynArray<byte>& bChecked, DynArray<int>& AffectedFunc)
{
  uint32 i;
  uint32 numFrags = Parser.m_CodeFragments.size();

  for (i=0; i<numFrags; i++)
  {
    if (bChecked[i])
      continue;
    SCodeFragment *s = &Parser.m_CodeFragments[i];
    if (!s->m_dwName)
    {
      bChecked[i] = 1;
      continue;
    }
    if (s->m_eType != eFT_Function)
      continue;

    gFind++;
    if (Parser.FindToken(pFunc->m_nFirstToken, pFunc->m_nLastToken, s->m_dwName) >= 0)
    {
      bChecked[i] = 1;
      AffectedFunc.push_back(i);
      CheckFunctionsAffect_r(Parser, s, bChecked, AffectedFunc);
    }
  }
}

void CShaderManBin::CheckStructureAffect_r(CParserBin& Parser, SCodeFragment *pFrag, DynArray<byte>& bChecked, DynArray<int>& AffectedFunc)
{
  uint32 i;
  uint32 numFrags = Parser.m_CodeFragments.size();

  for (i=0; i<numFrags; i++)
  {
    if (bChecked[i])
      continue;
    SCodeFragment *s = &Parser.m_CodeFragments[i];
    if (s->m_eType != eFT_Structure)
      continue;

    gFind++;
    if (Parser.FindToken(pFrag->m_nFirstToken, pFrag->m_nLastToken, s->m_dwName) >= 0)
    {
      bChecked[i] = 1;
      AffectedFunc.push_back(i);
      CheckStructureAffect_r(Parser, s, bChecked, AffectedFunc);
    }
  }
}

void CShaderManBin::CheckFragmentsAffect_r(CParserBin& Parser, DynArray<byte>& bChecked, DynArray<int>& AffectedFrags)
{
  uint32 i, j;
  uint32 numFuncs = AffectedFrags.size();
  uint32 numFrags = Parser.m_CodeFragments.size();

  for (i=0; i<numFuncs; i++)
  {
    int nFunc = AffectedFrags[i];
    SCodeFragment *pFunc = &Parser.m_CodeFragments[nFunc];
    for (j=0; j<numFrags; j++)
    {
      SCodeFragment *s = &Parser.m_CodeFragments[j];
      if (bChecked[j])
        continue;
      if (s->m_eType != eFT_Sampler && s->m_eType != eFT_Structure)
        continue;

      gFind++;
      if (Parser.FindToken(pFunc->m_nFirstToken, pFunc->m_nLastToken, s->m_dwName) >= 0)
      {
        bChecked[j] = 1;
        AffectedFrags.push_back(j);
        if (s->m_eType == eFT_Structure)
          CheckStructureAffect_r(Parser, s, bChecked, AffectedFrags);
      }
    }
  }
}

//=================================================================================================

struct SFXRegisterBin
{
  int nReg;
  int nComp;
  int m_nCB;
  uint32 m_nFlags;
  EParamType eType;
  uint32 dwName;
  string m_Value;
  SFXRegisterBin()
  {
    eType = eType_UNKNOWN;
    m_nCB = -1;
  }
};

struct SFXPackedName
{
  uint32 dwName[4];

  SFXPackedName()
  {
    dwName[0] = dwName[1] = dwName[2] = dwName[3] = 0;
  }
};

inline bool Compar(const SFXRegisterBin &a, const SFXRegisterBin &b)
{
  if (CParserBin::m_bD3D10)
  {
    if(a.m_nCB != b.m_nCB)
      return a.m_nCB < b.m_nCB;
  }
  if(a.nReg != b.nReg)
    return a.nReg < b.nReg;
  if(a.nComp != b.nComp)
    return a.nComp < b.nComp;
  return false;
}

static void sFlushRegs(CParserBin& Parser, int nReg, SFXRegisterBin *MergedRegs[4], DynArray<SFXParam>& NewParams, EHWShaderClass eSHClass, DynArray<SFXPackedName>& PackedNames)
{
  NewParams.resize(NewParams.size()+1);
  PackedNames.resize(PackedNames.size()+1);
  SFXParam& p = NewParams[NewParams.size()-1];
  SFXPackedName& pN = PackedNames[PackedNames.size()-1];
  int nMaxComp = -1;
  int j;

  int nCompMerged = 0;
  EParamType eType = eType_UNKNOWN;
  int nCB = -1;
  for (j=0; j<4; j++)
  {
    if (MergedRegs[j] && MergedRegs[j]->eType != eType_UNKNOWN)
    {
      if (nCB == -1)
        nCB = MergedRegs[j]->m_nCB;
      else
      if (nCB != MergedRegs[j]->m_nCB)
      {
        assert(0);
      }
      if (eType == eType_UNKNOWN)
        eType = MergedRegs[j]->eType;
      else
      if (eType != MergedRegs[j]->eType)
      {
        assert(0);
      }
    }
  }
  for (j=0; j<4; j++)
  {
    char s[16];
    sprintf(s, "__%d", j);
    p.m_Name += s;
    if (MergedRegs[j])
    {
      if (MergedRegs[j]->m_nFlags & PF_TWEAKABLE_MASK)
        p.m_nFlags |= (PF_TWEAKABLE_0 << j);
      p.m_nFlags |= MergedRegs[j]->m_nFlags & ~(PF_TWEAKABLE_MASK | PF_SCALAR | PF_SINGLE_COMP);
      nMaxComp = max(nMaxComp, j);
      pN.dwName[j] = MergedRegs[j]->dwName;
      if (nCompMerged)
        p.m_Values += ", ";
      p.m_Name += Parser.GetString(pN.dwName[j]);
      p.m_Values += MergedRegs[j]->m_Value;
      nCompMerged++;
    }
    else
    {
      if (nCompMerged)
        p.m_Values += ", ";
      p.m_Values += "NULL";
      nCompMerged++;
    }
  }
  p.m_nComps = nMaxComp+1;
  if (!CParserBin::m_bD3D10)
  {
    if (eSHClass == eHWSC_Geometry)
      return;
  }
  // Get packed name token
  p.m_dwName.push_back(Parser.NewUserToken(eT_unknown, p.m_Name.c_str(), true));
  p.m_nRegister[eSHClass] = nReg;
  p.m_nParameters = 1;
  p.m_eType = eType;
  p.m_nCB = nCB;
  p.m_nFlags |= PF_AUTOMERGED;
}

static EToken sCompToken[4] = {eT_x, eT_y, eT_z, eT_w};
bool CShaderManBin::ParseBinFX_Technique_Pass_PackParameters (CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, SCodeFragment *pFunc, EHWShaderClass eSHClass, uint32 dwSHName, DynArray<SFXParam>& PackedParams, DynArray<SCodeFragment>& Replaces, DynArray<uint32>& NewTokens)
{
  bool bRes = true;

  uint i, j;

  DynArray<SFXRegisterBin> Registers;
  DynArray<SFXPackedName> PackedNames;
  byte bMergeMask = (eSHClass == eHWSC_Pixel) ? 1 : 2;
  for (i=0; i<AffectedParams.size(); i++)
  {
    SFXParam *pr = &AffectedParams[i];
    if (pr->m_Annotations.empty())
      continue;  // Parameter doesn't have custom register definition
    if (pr->m_bWasMerged & bMergeMask)
      continue;
    pr->m_bWasMerged |= bMergeMask;
    char *src = (char *)pr->m_Annotations.c_str();
    char annot[256];
    while (true)
    {
      fxFill(&src, annot);
      if (!annot[0])
        break;
      if (strncmp(&annot[1], "sregister", 9)) 
        break;
      if ((eSHClass == eHWSC_Pixel && annot[0] != 'p') || (eSHClass == eHWSC_Vertex && annot[0] != 'v') || (eSHClass == eHWSC_Geometry && annot[0] != 'g'))
        continue;
      char *b = &annot[10];
      SkipCharacters (&b, kWhiteSpace);
      assert(b[0] == '=');
      b++;
      SkipCharacters (&b, kWhiteSpace);
      assert(b[0] == 'c');
      if (b[0] == 'c')
      {
        int nReg = atoi(&b[1]);
        b++;
        while (*b != '.')
        {
          if (*b == 0) // Vector without swizzling
            break;
          b++;
        }
        if (*b == '.')
        {
          pr->m_bWasMerged |= (bMergeMask << 4);
          b++;
          int nComp = -1;
          switch(b[0])
          {
          case 'x':
          case 'r': nComp = 0; break;
          case 'y':
          case 'g': nComp = 1; break;
          case 'z':
          case 'b': nComp = 2; break;
          case 'w':
          case 'a': nComp = 3; break;
          default:
            assert(0);
          }
          if (nComp >= 0)
          {
            pr->m_bWasMerged |= (bMergeMask << 6);
            SFXRegisterBin rg;
            rg.nReg = nReg;
            rg.nComp = nComp;
            rg.dwName = pr->m_dwName[0];
            rg.m_Value = pr->m_Values;
            rg.m_nFlags = pr->m_nFlags;
            rg.eType = (EParamType)pr->m_eType;
            rg.m_nCB = pr->m_nCB;
            assert(rg.m_Value[0]);
            Registers.push_back(rg);
          }
        }
      }
      break;
    }
  }
  if (Registers.empty())
    return false;
  std::sort(Registers.begin(), Registers.end(), Compar);
  int nReg = -1;
  int nCB = -1;
  SFXRegisterBin *MergedRegs[4];
  MergedRegs[0] = MergedRegs[1] = MergedRegs[2] = MergedRegs[3] = NULL;
  for (i=0; i<Registers.size(); i++)
  {
    SFXRegisterBin *rg = &Registers[i];
    if ((!CParserBin::m_bD3D10 && rg->nReg != nReg) || (CParserBin::m_bD3D10 && (rg->m_nCB != nCB || rg->nReg != nReg)))
    {
      if (nReg >= 0)
        sFlushRegs(Parser, nReg, MergedRegs, PackedParams, eSHClass, PackedNames);
      nReg = rg->nReg;
      nCB = rg->m_nCB;
      MergedRegs[0] = MergedRegs[1] = MergedRegs[2] = MergedRegs[3] = NULL;
    }
    assert(!MergedRegs[rg->nComp]);
    if (MergedRegs[rg->nComp])
    {
      Warning("register c%d (comp: %d) is used by the %s shader '%s' already", rg->nReg, rg->nComp, eSHClass==eHWSC_Pixel ? "pixel" : "vertex", Parser.GetString(dwSHName));
      assert(0);
    }
    MergedRegs[rg->nComp] = rg;
  }
  if (MergedRegs[0] || MergedRegs[1] || MergedRegs[2] || MergedRegs[3])
    sFlushRegs(Parser, nReg, MergedRegs, PackedParams, eSHClass, PackedNames);

  // Replace new parameters in shader tokens
  for (int n=0; n<AffectedFunc.size(); n++)
  {
    SCodeFragment *st = &Parser.m_CodeFragments[AffectedFunc[n]];
    const char *szName = Parser.GetString(st->m_dwName);

    for (i=0; i<PackedParams.size(); i++)
    {
      SFXParam *pr = &PackedParams[i];
      SFXPackedName *pn = &PackedNames[i];
      for (j=0; j<4; j++)
      {
        uint32 nP = pn->dwName[j];
        if (nP == 0)
          continue;
        int32 nPos = st->m_nFirstToken;
        while (true)
        {
          nPos = Parser.FindToken(nPos, st->m_nLastToken, nP);
          if (nPos < 0)
            break;
          SCodeFragment Fr;
          Fr.m_nFirstToken = Fr.m_nLastToken = nPos;
          Fr.m_dwName = n;
          Replaces.push_back(Fr);

          Fr.m_nFirstToken = NewTokens.size();
          NewTokens.push_back(pr->m_dwName[0]);
          NewTokens.push_back(eT_dot);
          NewTokens.push_back(sCompToken[j]);
          Fr.m_nLastToken = NewTokens.size()-1;
          Replaces.push_back(Fr);
          nPos++;
        }
      }
    }
  }

  for (i=0; i<Replaces.size(); i+=2)
  {
    SCodeFragment *pFR1 = &Replaces[i];
    for (j=i+2; j<Replaces.size(); j+=2)
    {
      SCodeFragment *pFR2 = &Replaces[j];
      if (pFR1->m_dwName != pFR2->m_dwName)
        continue;
      if (pFR2->m_nFirstToken < pFR1->m_nFirstToken)
      {
        std::swap(Replaces[i], Replaces[j]);
        std::swap(Replaces[i+1], Replaces[j+1]);
      }
    }
  }

  return bRes;
}

//===================================================================================================

inline bool CompareVars(const SFXParam *a, const SFXParam *b)
{
  uint16 nCB0 = CB_MAX;
  uint16 nCB1 = CB_MAX;
  int16 nReg0 = 10000;
  int16 nReg1 = 10000;
  nCB0 = a->m_nCB;
  nReg0 = a->m_nRegister[gRenDev->m_RP.m_FlagsShader_LT];

  nCB1 = b->m_nCB;
  nReg1 = b->m_nRegister[gRenDev->m_RP.m_FlagsShader_LT];

  if (nCB0 != nCB1)
    return (nCB0 < nCB1);
  return (nReg0 < nReg1);
}
EToken dwNamesCB[CB_MAX] = {eT_PER_BATCH, eT_PER_INSTANCE, eT_PER_FRAME, eT_PER_MATERIAL, eT_PER_LIGHT, eT_PER_SHADOWGEN, eT_SKIN_DATA, eT_SHAPE_DATA, eT_INSTANCE_DATA};

void CShaderManBin::AddParameterToScript(CParserBin& Parser, SFXParam *pr, DynArray<uint32>& SHData, EHWShaderClass eSHClass, int nCB)
{
  char str[256];
  int nReg = pr->m_nRegister[eSHClass];
  if (pr->m_eType == eType_BOOL)
    SHData.push_back(eT_bool);
  else
  if (pr->m_eType == eType_INT)
    SHData.push_back(eT_int);
  else
  {
    int nVal = pr->m_nParameters*4+pr->m_nComps;
    EToken eT = eT_unknown;
    switch (nVal)
    {
      case 4+1:
        eT = eT_float;
        break;
      case 4+2:
        eT = eT_float2;
        break;
      case 4+3:
        eT = eT_float3;
        break;
      case 4+4:
        eT = eT_float4;
        break;
      case 2*4+4:
        eT = eT_float2x4;
        break;
      case 3*4+4:
        eT = eT_float3x4;
        break;
      case 4*4+4:
        eT = eT_float4x4;
        break;
      case 3*4+3:
        eT = eT_float3x3;
        break;
    }
    assert(eT != eT_unknown);
    if (eT == eT_unknown)
      return;
    SHData.push_back(eT);
  }
  for (int i=0; i<pr->m_dwName.size(); i++)
    SHData.push_back(pr->m_dwName[i]);

  if (nReg>=0 && nReg<10000)
  {
    SHData.push_back(eT_colon);
    if (nCB == CB_PER_MATERIAL || nCB == CB_PER_FRAME || nCB == CB_PER_LIGHT || nCB == CB_PER_SHADOWGEN)
      SHData.push_back(eT_packoffset);
    else
      SHData.push_back(eT_register);
    SHData.push_back(eT_br_rnd_1);
    sprintf(str, "c%d", nReg);
    SHData.push_back(Parser.NewUserToken(eT_unknown, str, true));
    SHData.push_back(eT_br_rnd_2);
  }
  SHData.push_back(eT_semicolumn);

#if defined(PS3)
  sprintf(str, "CBUFFER(%d)\n",pr->m_nCB);
  SHData.push_back(eT_comment);
  SHData.push_back(Parser.NewUserToken(eT_unknown, str, true));
#endif
}

static void sParseCSV(string& sFlt, std::vector<string>& Filters)
{
  const char *cFlt = sFlt.c_str();
  char Flt[64];
  int nFlt = 0;
  while(true)
  {
    char c = *cFlt++;
    if (!c)
      break;
    if (sSkipChars[c])
    {
      if (nFlt)
      {
        Flt[nFlt] = 0;
        Filters.push_back(string(Flt));
        nFlt = 0;
      }
      continue;
    }
    Flt[nFlt++] = c;
  }
  if (nFlt)
  {
    Flt[nFlt] = 0;
    Filters.push_back(string(Flt));
  }
}

void CShaderManBin::GeneratePublicParameters(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<uint32>& NewTokens)
{
  if (Parser.m_pCurShader)
  {
    for (int i=0; i<AffectedParams.size(); i++)
    {
      SFXParam *pr = &AffectedParams[i];
      uint nFlags = pr->GetParamFlags();
      if (nFlags & PF_AUTOMERGED)
        continue;
      if (nFlags & PF_TWEAKABLE_MASK)
      {
        const char *szName = Parser.GetString(pr->m_dwName[0]);
        // Avoid duplicating of public parameters
        uint j;
        for (j=0; j<Parser.m_pCurShader->m_PublicParams.size(); j++)
        {
          SShaderParam *p = &Parser.m_pCurShader->m_PublicParams[j];
          if (!stricmp(p->m_Name, szName))
            break;
        }
        if (j == Parser.m_pCurShader->m_PublicParams.size())
        {
          SShaderParam sp;
          strncpy(sp.m_Name, szName, 32);
          EParamType eType;
          string szWidget = pr->GetValueForName("UIWidget", eType);
          const char *szVal = pr->m_Values.c_str();
          if (szWidget == "color")
          {
            sp.m_Type = eType_FCOLOR;
            if (szVal[0] == '{')
              szVal++;
            int n = sscanf(szVal, "%f, %f, %f, %f", &sp.m_Value.m_Color[0], &sp.m_Value.m_Color[1], &sp.m_Value.m_Color[2], &sp.m_Value.m_Color[3]);
            assert(n == 4);
          }
          else
          {
            sp.m_Type = eType_FLOAT;
            sp.m_Value.m_Float = (float)atof(szVal);
          }
          if (!pr->m_Annotations.empty() && gRenDev->m_bEditor)
          {
            EParamType eType;
            string sFlt = pr->GetValueForName("Filter", eType);
            bool bUseScript = true;
            if (!sFlt.empty())
            {
              std::vector<string> Filters;
              sParseCSV(sFlt, Filters);
              string strShader = Parser.m_pCurShader->GetName();
              int i;
              for (i=0; i<Filters.size(); i++)
              {
                if (!strnicmp(Filters[i].c_str(), strShader.c_str(), Filters[i].size()))
                  break;
              }
              if (i == Filters.size())
                bUseScript = false;
            }
            if (bUseScript)
              sp.m_Script = pr->m_Annotations;
            else
              sp.m_Type = eType_UNKNOWN;
          }
          Parser.m_pCurShader->m_PublicParams.push_back(sp);
        }
      }
    }
  }
}

bool CShaderManBin::ParseBinFX_Technique_Pass_GenerateShaderData(CParserBin& Parser, FXMacroBin& Macros, DynArray<SFXParam>& AffectedParams, uint32 dwSHName, EHWShaderClass eSHClass, uint32& nAffectMask, uint32 dwSHType, DynArray<uint32>& SHData, SShaderTechnique *pShTech)
{
  bool bRes = true;

  int i, nNum;
  static DynArray<int> AffectedFragments;
  AffectedFragments.resize(0);

  for (nNum=0; nNum<Parser.m_CodeFragments.size(); nNum++)
  {
    if (dwSHName == Parser.m_CodeFragments[nNum].m_dwName)
      break;
  }
  if (nNum == Parser.m_CodeFragments.size())
  {
    Warning("Couldn't find entry function '%s'", Parser.GetString(dwSHName));
    assert(0);
    return false;
  }

  AffectedFragments.push_back(nNum);
  SCodeFragment *pFunc = &Parser.m_CodeFragments[nNum];
  DynArray<byte> bChecked;
  bChecked.resize(Parser.m_CodeFragments.size());
  if (bChecked.size() > 0)
    memset(&bChecked[0], 0, sizeof(byte)*bChecked.size());
  bChecked[nNum] = 1;
  CheckFunctionsAffect_r(Parser, pFunc, bChecked, AffectedFragments);
  CheckFragmentsAffect_r(Parser, bChecked, AffectedFragments);

  nAffectMask = 0;
  for (i=0; i<AffectedFragments.size(); i++)
  {
    SCodeFragment *s = &Parser.m_CodeFragments[AffectedFragments[i]];
    if (s->m_eType != eFT_Function && s->m_eType != eFT_Structure)
      continue;
    FXMacroBinItor itor;
    for (itor=Macros.begin(); itor!=Macros.end(); itor++)
    {
      SMacroBinFX *pr = &itor->second;
      if (!pr->m_nMask)
        continue;
      if (pr->m_nMask & nAffectMask)
        continue;
      uint32 dwName = itor->first;
      if (Parser.FindToken(s->m_nFirstToken, s->m_nLastToken, dwName) >= 0)
        nAffectMask |= pr->m_nMask;
    }
  }

  // Generate list of params before first preprocessor pass for affected functions
  for (i=0; i<Parser.m_Parameters.size(); i++)
  {
    SFXParam *pr = &Parser.m_Parameters[i];
    pr->m_bAffected = false;
    pr->m_bWasMerged = 0;
  }

  for (i=0; i<Parser.m_Parameters.size(); i++)
  {
    SFXParam *pr = &Parser.m_Parameters[i];
    AddAffectedParameter(Parser, AffectedParams, AffectedFragments, pr, eSHClass, dwSHType, pShTech);
  }

  FXMacroBinItor itor;
  for (itor=Macros.begin(); itor!=Macros.end(); itor++)
  {
    if (itor->second.m_nMask && !(itor->second.m_nMask & nAffectMask))
      continue;
    SHData.push_back(eT_define);
    SHData.push_back(itor->first);
    SHData.push_back(0);
  }
  DynArray<SFXParam> PackedParams;
  DynArray<SCodeFragment> Replaces;
  DynArray<uint32> NewTokens;
  ParseBinFX_Technique_Pass_PackParameters (Parser, AffectedParams, AffectedFragments, pFunc, eSHClass, dwSHName, PackedParams, Replaces, NewTokens);

  // Update new parameters in shader structures
  for (i=0; i<PackedParams.size(); i++)
  {
    SFXParam *pr = &PackedParams[i];
    AffectedParams.push_back(*pr);
  }

  // Include all affected functions/structures/parameters in the final script 
	//need CB-scopes on PS3 for correct reflection, but everything else d3d9-HLSL-style
#if defined(PS3)
	if(false)
#else
  if (CParserBin::m_bD3D10) // use CB scopes for D3D10 shaders
#endif
  {
    int8 nPrevCB = -1;
    DynArray<SFXParam *> ParamsData;

    byte bMergeMask = (eSHClass == eHWSC_Pixel) ? 1 : 2;
    bMergeMask <<= 4;
    for (i=0; i<AffectedParams.size(); i++)
    {
      SFXParam &pr = AffectedParams[i];
      if (pr.m_bWasMerged & bMergeMask)
        continue;
      if (pr.m_nCB >= 0)
        ParamsData.push_back(&pr);
    }
    if (eSHClass == eHWSC_Vertex)
      gRenDev->m_RP.m_FlagsShader_LT = 0;
    else
    if (eSHClass == eHWSC_Pixel)
      gRenDev->m_RP.m_FlagsShader_LT = 1;
    else
      gRenDev->m_RP.m_FlagsShader_LT = 2;

    std::sort(ParamsData.begin(), ParamsData.end(), CompareVars);

    // First we need to declare semantic variables (in CB scopes in case of DX10)
    for (i=0; i<ParamsData.size(); i++)
    {
      SFXParam *pPr = ParamsData[i];
      int nCB = pPr->m_nCB;
      nCB = pPr->m_nCB;
      if (nPrevCB != nCB)
      {
        if (nPrevCB != -1)
        {
          SHData.push_back(eT_br_cv_2);
          SHData.push_back(eT_semicolumn);
        }
        SHData.push_back(eT_cbuffer);
        SHData.push_back(dwNamesCB[nCB]);
        SHData.push_back(eT_colon);
        SHData.push_back(eT_register);
        char str[32];
        sprintf(str, "b%d", nCB);
        SHData.push_back(eT_br_rnd_1);
        SHData.push_back(Parser.NewUserToken(eT_unknown, str, true));
        SHData.push_back(eT_br_rnd_2);
        SHData.push_back(eT_br_cv_1);
      }
      nPrevCB = nCB;
      AddParameterToScript(Parser, pPr, SHData, eSHClass, nCB);
    }
    if (nPrevCB >= 0)
    {
      nPrevCB = -1;
      SHData.push_back(eT_br_cv_2);
      SHData.push_back(eT_semicolumn);
    }
  }
  else
  {
    // Update affected parameters in script
    byte bMergeMask = (eSHClass == eHWSC_Pixel) ? 1 : 2;
    bMergeMask <<= 4;
    for (i=0; i<AffectedParams.size(); i++)
    {
      SFXParam *pr = &AffectedParams[i];
      // Ignore parameters which where packed
      if (pr->m_bWasMerged & bMergeMask)
        continue;
      AddParameterToScript(Parser, pr, SHData, eSHClass, -1);
    }
  }
   
  // Generate global public parameters list for the shader
  // From affected params
  GeneratePublicParameters(Parser, AffectedParams, NewTokens);

  // Generate fragment tokens
  int j = -1;
  for (i=0; i<Parser.m_CodeFragments.size(); i++)
  {
    SCodeFragment *cf = &Parser.m_CodeFragments[i];
    if (cf->m_dwName)
    {
      for (j=0; j<AffectedFragments.size(); j++)
      {
        if (AffectedFragments[j] == i)
          break;
      }
      if (j == AffectedFragments.size())
        continue;
    }

    Parser.CopyTokens(cf, SHData, Replaces, NewTokens, j);
    if (cf->m_eType == eFT_Sampler)
      SHData.push_back(eT_semicolumn);
  }

  return bRes;
}

bool CShaderManBin::ParseBinFX_Technique_Pass_LoadShader(CParserBin& Parser, FXMacroBin& Macros, SParserFrame& SHFrame, SShaderTechnique *pShTech, SShaderPass *pPass, EHWShaderClass eSHClass)
{
  bool bRes = true;

#ifndef NULL_RENDERER

  assert(!SHFrame.IsEmpty());

  uint32 dwSHName;
  uint32 dwSHType = 0;

  uint32 *pTokens = &Parser.m_Tokens[0];

  EHWSProfile eSHV;
  if (eSHClass == eHWSC_Pixel)
    eSHV = eHWSP_PS_1_1;
  else
  if (eSHClass == eHWSC_Vertex)
    eSHV = eHWSP_VS_1_1;
  else
  if (eSHClass == eHWSC_Geometry)
    eSHV = eHWSP_GS_4_0;
  uint32 nTok = pTokens[SHFrame.m_nFirstToken];
  assert(nTok == eT_compile);
  if (nTok == eT_compile)
  {
    nTok = pTokens[SHFrame.m_nFirstToken+1];
    switch (nTok)
    {
      case eT_ps_1_1:
        eSHV = eHWSP_PS_1_1;
        break;
      case eT_ps_2_0:
        eSHV = eHWSP_PS_2_0;
        break;
      case eT_ps_2_a:
      case eT_ps_2_b:
      case eT_ps_2_x:
        eSHV = eHWSP_PS_2_X;
        break;
      case eT_ps_3_0:
        eSHV = eHWSP_PS_3_0;
        break;
      case eT_ps_4_0:
        eSHV = eHWSP_PS_4_0;
        break;
      case eT_ps_Auto:
        eSHV = eHWSP_PS_Auto;
        break;

      case eT_vs_1_1:
        eSHV = eHWSP_VS_1_1;
        break;
      case eT_vs_2_0:
        eSHV = eHWSP_VS_2_0;
        break;
      case eT_vs_3_0:
        eSHV = eHWSP_VS_3_0;
        break;
      case eT_vs_4_0:
        eSHV = eHWSP_VS_4_0;
        break;
      case eT_vs_Auto:
        eSHV = eHWSP_VS_Auto;
        break;

      case eT_gs_4_0:
        eSHV = eHWSP_GS_4_0;
        break;
      default:
        assert(0);
    }
    dwSHName = pTokens[SHFrame.m_nFirstToken+2];
    nTok = pTokens[SHFrame.m_nFirstToken+3];
    assert (nTok == eT_br_rnd_1);
    int nCur = SHFrame.m_nFirstToken+4;
    if (nTok == eT_br_rnd_1)
    {
      nTok = pTokens[nCur];
      if (nTok != eT_br_rnd_2)
      {
        assert(!"Local function parameters aren't supported anymore");
      }
    }
    nCur++;
    if (nCur <= SHFrame.m_nLastToken)
    {
      dwSHType = pTokens[nCur];
    }
  }

  uint nGenMask = 0;
  DynArray<SFXParam> AffectedParams;
  DynArray<uint32> SHData;
  const char *szName = Parser.GetString(dwSHName);
  bRes &= ParseBinFX_Technique_Pass_GenerateShaderData(Parser, Macros, AffectedParams, dwSHName, eSHClass, nGenMask, dwSHType, SHData, pShTech);

  CHWShader *pSH = NULL;
  bool bValidShader = false;

  CShader *efSave = gRenDev->m_RP.m_pShader;
  gRenDev->m_RP.m_pShader = Parser.m_pCurShader;
  if (bRes && !SHData.empty())
  {
    char str[1024];
#if defined(XENON)
    sprintf(str, "%s/%s", Parser.m_pCurShader->m_NameShader.c_str(), szName);
#else
    sprintf(str, "%s@%s", Parser.m_pCurShader->m_NameShader.c_str(), szName);
#endif
    pSH = CHWShader::mfForName(str, Parser.m_pCurShader->m_NameFile.c_str(), Parser.m_pCurShader->m_CRC32, Parser.m_Samplers, AffectedParams, dwSHName, eSHClass, eSHV, SHData, Parser.m_Table, dwSHType, nGenMask, Parser.m_pCurShader->m_nMaskGenFX);
  }
  if (pSH)
  {
    bValidShader = true;

    if (eSHClass == eHWSC_Vertex)
      pPass->m_VShader = pSH;
    else
    if (eSHClass == eHWSC_Pixel)
		{
#if defined(PS3)
			//PS3HACK debug
			if(!pSH)
			{
				int a=0;
			}
#endif
      pPass->m_PShader = pSH;
		}
    else
    if (CParserBin::m_bD3D10 && eSHClass == eHWSC_Geometry)
      pPass->m_GShader = pSH;
    else
      assert(0);
  }

  gRenDev->m_RP.m_pShader = efSave;
#endif

  return bRes;
}

bool CShaderManBin::ParseBinFX_Technique_Pass(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pShTech)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(VertexShader)
    FX_TOKEN(PixelShader)
    FX_TOKEN(GeometryShader)
    FX_TOKEN(ZEnable)
    FX_TOKEN(ZWriteEnable)
    FX_TOKEN(CullMode)
    FX_TOKEN(SrcBlend)
    FX_TOKEN(DestBlend)
    FX_TOKEN(AlphaBlendEnable)
    FX_TOKEN(AlphaFunc)
    FX_TOKEN(AlphaRef)
    FX_TOKEN(ZFunc)
    FX_TOKEN(ColorWriteEnable)
    FX_TOKEN(IgnoreMaterialState)
  FX_END_TOKENS

  bool bRes = true;
  int n = pShTech->m_Passes.Num();
  pShTech->m_Passes.ReserveNew(n+1);
  SShaderPass *sm = &pShTech->m_Passes[n];
  sm->m_eCull = -1;
  sm->m_AlphaRef = ~0;

  SParserFrame VS, PS, GS;
  FXMacroBin VSMacro, PSMacro, GSMacro;

  byte ZFunc = eCF_LEqual;

  byte ColorWriteMask = 0xff;

  byte AlphaFunc = eCF_Disable;
  byte AlphaRef = 0;
  int State = GS_DEPTHWRITE;

  int nMaxTMU = 0;
  signed char Cull = -1;
  int nIndex;
  EToken eSrcBlend = eT_unknown;
  EToken eDstBlend = eT_unknown;
  bool bBlend = false;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_VertexShader:
        VS = Parser.m_Data;
        VSMacro = Parser.m_Macros[1];
        break;
      case eT_PixelShader:
        PS = Parser.m_Data;
        PSMacro = Parser.m_Macros[1];
        break;
      case eT_GeometryShader:
        GS = Parser.m_Data;
        GSMacro = Parser.m_Macros[1];
        break;

      case eT_ZEnable:
        if (Parser.GetBool(Parser.m_Data))
          State &= ~GS_NODEPTHTEST;
        else
          State |= GS_NODEPTHTEST;
        break;
      case eT_ZWriteEnable:
        if (Parser.GetBool(Parser.m_Data))
          State |= GS_DEPTHWRITE;
        else
          State &= ~GS_DEPTHWRITE;
        break;
      case eT_CullMode:
        eT = Parser.GetToken(Parser.m_Data);
        if (eT == eT_None)
          Cull = eCULL_None;
        else
        if (eT == eT_CCW || eT == eT_Back)
          Cull = eCULL_Back;
        else
        if (eT == eT_CW || eT == eT_Front)
          Cull = eCULL_Front;
        else
        {
          Warning("unknown CullMode parameter '%s' (Skipping)\n", Parser.GetString(eT));
          assert(0);
        }
        break;
      case eT_AlphaFunc:
        AlphaFunc = Parser.GetCompareFunc(Parser.GetToken(Parser.m_Data));
        break;
    case eT_ColorWriteEnable:
      {
        if (ColorWriteMask == 0xff)
          ColorWriteMask = 0;
        int nCur = Parser.m_Data.m_nFirstToken;
        while (nCur <= Parser.m_Data.m_nLastToken)
        {
          uint32 nT = Parser.m_Tokens[nCur++];
          if (nT == eT_or)
            continue;
          if (nT == eT_0)
            ColorWriteMask |= 0;
          else
          if (nT == eT_RED)
            ColorWriteMask |= 1;
          else
          if (nT == eT_GREEN)
            ColorWriteMask |= 2;
          else
          if (nT == eT_BLUE)
            ColorWriteMask |= 4;
          else
          if (nT == eT_ALPHA)
            ColorWriteMask |= 8;
          else
          {
            Warning("unknown WriteMask parameter '%s' (Skipping)\n", Parser.GetString(eT));
          }
        }
      }
      break;
    case eT_ZFunc:
      ZFunc = Parser.GetCompareFunc(Parser.GetToken(Parser.m_Data));
      sm->m_PassFlags |= SHPF_FORCEZFUNC;
      break;
    case eT_AlphaRef:
      AlphaRef = Parser.GetInt(Parser.GetToken(Parser.m_Data));
      break;
    case eT_SrcBlend:
      eSrcBlend = Parser.GetToken(Parser.m_Data);
      break;
    case eT_DestBlend:
      eDstBlend = Parser.GetToken(Parser.m_Data);
      break;
    case eT_AlphaBlendEnable:
      bBlend = Parser.GetBool(Parser.m_Data);
      break;

    case eT_IgnoreMaterialState:
      sm->m_PassFlags |= SHPF_NOMATSTATE;
      break;

    default:
      assert(0);
    }
  }

  if (bBlend && eSrcBlend && eDstBlend)
  {
    int nSrc = Parser.GetSrcBlend(eSrcBlend);
    int nDst = Parser.GetDstBlend(eDstBlend);
    if (nSrc >= 0 && nDst >= 0)
    {
      State |= nSrc;
      State |= nDst;
    }
  }
  if (ColorWriteMask != 0xff)
  {
    for (int i=0; i<4; i++)
    {
      if (!(ColorWriteMask & (1<<i)))
        State |= GS_NOCOLMASK_R<<i;
    }
  }

  if (AlphaFunc)
  {
    switch (AlphaFunc)
    {
      case eCF_Greater:
        State |= GS_ALPHATEST_GREATER;
        break;
      case eCF_GEqual:
        State |= GS_ALPHATEST_GEQUAL;
        break;
      case eCF_Less:
        State |= GS_ALPHATEST_LESS;
        break;
      case eCF_LEqual:
        State |= GS_ALPHATEST_LEQUAL;
        break;
      default:
        assert(0);
    }
  }

  switch (ZFunc)
  {
    case eCF_Equal:
      State |= GS_DEPTHFUNC_EQUAL;
      break;
    case eCF_Greater:
      State |= GS_DEPTHFUNC_GREAT;
      break;
    case eCF_GEqual:
      State |= GS_DEPTHFUNC_GEQUAL;
      break;
    case eCF_Less:
      State |= GS_DEPTHFUNC_LESS;
      break;
    case eCF_NotEqual:
      State |= GS_DEPTHFUNC_NOTEQUAL;
      break;
    case eCF_LEqual:
      State |= GS_DEPTHFUNC_LEQUAL;
      break;
    default:
      assert(0);
  }

  sm->m_RenderState = State;
  sm->m_eCull = Cull;

  if (!VS.IsEmpty())
    bRes &= ParseBinFX_Technique_Pass_LoadShader(Parser, VSMacro, VS, pShTech, sm, eHWSC_Vertex);
  if (!PS.IsEmpty())
    bRes &= ParseBinFX_Technique_Pass_LoadShader(Parser, PSMacro, PS, pShTech, sm, eHWSC_Pixel);
#if  !defined(PS3)
  if (CParserBin::m_bD3D10 && !GS.IsEmpty())
    bRes &= ParseBinFX_Technique_Pass_LoadShader(Parser, GSMacro, GS, pShTech, sm, eHWSC_Geometry);
#endif

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_LightStyle_Val(CParserBin& Parser, SParserFrame& Frame, CLightStyle *ls)
{
  int i;
  char str[64];
  const char *pstr1, *pstr2;
  ColorF col;

  col = Col_Black;

  ls->m_Map.Free();
  string sr = Parser.GetString(Frame);
  const char *lstr = sr.c_str();

  int n = 0;
  while (true)
  {
    pstr1 = strchr(lstr, '|');
    if (!pstr1)
      break;
    pstr2 = strchr(pstr1+1, '|');
    if (!pstr2)
      break;
    if (pstr2-pstr1-1 > 0)
    {
      strncpy(str, pstr1+1, pstr2-pstr1-1);
      str[pstr2-pstr1-1] = 0;
      i = sscanf(str, "%f %f %f %f", &col[0], &col[1], &col[2], &col[3]);
      switch (i)
      {
      default:
        break;

      case 1:
        col[1] = col[2] = col[0];
        col[3] = 1.0f;
        break;

      case 2:
        col[2] = 1.0f;
        col[3] = 1.0f;
        break;

      case 3:
        col[3] = 1.0f;
        break;
      }
      ls->m_Map.AddElem(col);
      n++;
    }

    lstr = pstr2;
  }
  ls->m_Map.Shrink();
  assert(ls->m_Map.Num() == n);
  if (ls->m_Map.Num() == n)
    return true;
  return false;
}

bool CShaderManBin::ParseBinFX_LightStyle(CParserBin& Parser, SParserFrame& Frame, int nStyle)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(ValueString)
    FX_TOKEN(Speed)
  FX_END_TOKENS

  bool bRes = true;

  Parser.m_pCurShader->m_Flags |= EF_LIGHTSTYLE;
  if (CLightStyle::m_LStyles.Num() <= (uint)nStyle)
    CLightStyle::m_LStyles.ReserveNew(nStyle+1);
  CLightStyle *ls = CLightStyle::m_LStyles[nStyle];
  if (!ls)
  {
    ls = new CLightStyle;
    ls->m_LastTime = 0;
    ls->m_Color = Col_White;
    CLightStyle::m_LStyles[nStyle] = ls;
  }
  ls->m_TimeIncr = 60;
  int nIndex;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
    case eT_ValueString:
      bRes &= ParseBinFX_LightStyle_Val(Parser, Parser.m_Data, ls);
      break;

    case eT_Speed:
      ls->m_TimeIncr = Parser.GetFloat(Parser.m_Data);
      break;

    default:
      assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Technique_Annotations_String(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pShTech, DynArray<SShaderTechParseParams>& techParams, bool *bPublic)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(Public)
    FX_TOKEN(NoLights)
    FX_TOKEN(NoMaterialState)
    FX_TOKEN(PositionInvariant)
    FX_TOKEN(TechniqueZ)
    FX_TOKEN(TechniqueScatterPass)
    FX_TOKEN(TechniqueShadowPass)
    FX_TOKEN(TechniqueShadowGen)
    FX_TOKEN(TechniqueShadowGenDX10)
    FX_TOKEN(TechniqueDetail)
    FX_TOKEN(TechniqueCaustics)
    FX_TOKEN(TechniqueGlow)
    FX_TOKEN(TechniqueMotionBlur)
    FX_TOKEN(TechniqueCustomRender)
    FX_TOKEN(TechniqueRainPass)
  FX_END_TOKENS

  bool bRes = true;
  int nIndex;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_Public:
        if (pShTech)
          pShTech->m_Flags |= FHF_PUBLIC;
        else
        if (bPublic)
          *bPublic = true;
        break;

      case eT_PositionInvariant:
        if (pShTech)
          pShTech->m_Flags |= FHF_POSITION_INVARIANT;
        break;

      case eT_NoLights:
        if (pShTech)
          pShTech->m_Flags |= FHF_NOLIGHTS;
        break;

      case eT_NoMaterialState:
        if (Parser.m_pCurShader)
          Parser.m_pCurShader->m_Flags2 |= EF2_IGNORERESOURCESTATES;
        break;

      case eT_TechniqueZ:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameZ = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_Z] = nameZ;
        }
        break;
      case eT_TechniqueScatterPass:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameScatterPass = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_SCATTERPASS] = nameScatterPass;
        }
        break;
      case eT_TechniqueShadowGen:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameGenShadow = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_SHADOWGEN] = nameGenShadow;
        }
        break;
      case eT_TechniqueShadowGenDX10:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameGenShadow = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_SHADOWGENGS] = nameGenShadow;
        }
        break;
      case eT_TechniqueShadowPass:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameShadow = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_SHADOWPASS] = nameShadow;
        }
        break;
      case eT_TechniqueCaustics:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName name = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_CAUSTICS] = name;
        }
        break;
      case eT_TechniqueDetail:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameDetail = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_DETAIL] = nameDetail;
        }
        break;

      case eT_TechniqueGlow:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameGlow = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_GLOWPASS] = nameGlow;
        }
        break;
      case eT_TechniqueMotionBlur:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameMotionBlur = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_MOTIONBLURPASS] = nameMotionBlur;
        }
        break;
      case eT_TechniqueCustomRender:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameTech = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_CUSTOMRENDERPASS] = nameTech;
        }
        break;
      case eT_TechniqueRainPass:
        {
          if (!Parser.m_pCurShader)
            break;
          CCryName nameTech = Parser.GetString(Parser.m_Data).c_str();
          int nIndex = techParams.size()-1;
          assert(nIndex >= 0);
          techParams[nIndex].techName[TTYPE_RAINPASS] = nameTech;
        }
        break;

      default:
        assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Technique_Annotations(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pShTech, DynArray<SShaderTechParseParams>& techParams, bool *bPublic)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(string)
  FX_END_TOKENS

  bool bRes = true;
  int nIndex;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
    case eT_string:
      {
        eT = Parser.GetToken(Parser.m_Name);
        assert(eT == eT_Script);
        bRes &= ParseBinFX_Technique_Annotations_String(Parser, Parser.m_Data, pShTech, techParams, bPublic);
      }
      break;

    default:
      assert(0);
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Technique_CustomRE(CParserBin& Parser, SParserFrame& Frame, SParserFrame& Name, SShaderTechnique *pShTech)
{
  uint32 nName = Parser.GetToken(Name);

  if (nName == eT_Flare)
  {
    CREFlare *ps = new CREFlare;
    if (ps->mfCompile(Parser, Frame))
    {
      pShTech->m_REs.AddElem(ps);
      pShTech->m_Flags |= FHF_RE_FLARE;
      return true;
    }
    else
      delete ps;
  }
  else
  if (nName == eT_Cloud)
  {
    CRECloud *ps = new CRECloud;
    if (ps->mfCompile(Parser, Frame))
    {
      pShTech->m_REs.AddElem(ps);
      pShTech->m_Flags |= FHF_RE_CLOUD;
      return true;
    }
    else
      delete ps;
  }
  else
  if (nName == eT_Beam)
  {
    CREBeam *ps = new CREBeam;
    if (ps->mfCompile(Parser, Frame))
    {
      pShTech->m_REs.AddElem(ps);
    }
    else
      delete ps;
  }
  else
  if (nName == eT_Ocean)
  {
#if !defined(XENON) && !defined(PS3)
    CREOcean *ps = new CREOcean;
    if (ps->mfCompile(Parser, Frame))
    {
      pShTech->m_REs.AddElem(ps);
    }
    else
      delete ps;
#else
    assert(0);
#endif
  }

  return true;
}

SShaderTechnique *CShaderManBin::ParseBinFX_Technique(CParserBin& Parser, SParserFrame& Frame, SParserFrame Annotations, DynArray<SShaderTechParseParams>& techParams, bool *bPublic)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(pass)
    FX_TOKEN(CustomRE)
    FX_TOKEN(Style)
  FX_END_TOKENS

  bool bRes = true;
  SShaderTechnique *pShTech = NULL;
  if (Parser.m_pCurShader)
    pShTech = new SShaderTechnique;

  if (Parser.m_pCurShader)
  {
    SShaderTechParseParams ps;
    techParams.push_back(ps);
  }

  if (!Annotations.IsEmpty())
    ParseBinFX_Technique_Annotations(Parser, Annotations, pShTech, techParams, bPublic);

  while (Parser.ParseObject(sCommands))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_pass:
        if (pShTech)
          bRes &= ParseBinFX_Technique_Pass(Parser, Parser.m_Data, pShTech);
        break;

      case eT_Style:
        if (pShTech)
          ParseBinFX_LightStyle(Parser, Parser.m_Data, Parser.GetInt(Parser.GetToken(Parser.m_Name)));
        break;

      case eT_CustomRE:
        if (pShTech)
          bRes &= ParseBinFX_Technique_CustomRE(Parser, Parser.m_Data, Parser.m_Name, pShTech);
        break;

      default:
        assert(0);
    }
  }

  if (bRes)
  {
    if (Parser.m_pCurShader && pShTech)
    {
      Parser.m_pCurShader->m_HWTechniques.AddElem(pShTech);
    }
  }
  else
  {
    techParams.pop_back();
  }

  Parser.EndFrame(OldFrame);

  return pShTech;
}

bool CShaderManBin::ParseBinFX(SShaderBin *pBin, CShader *ef, CShader *efGen, uint32 nMaskGen)
{
  bool bRes = true;
  CParserBin Parser(pBin, ef);

  TArray<char> custMacros;
  if (efGen && efGen->m_ShaderGenParams)
    AddGenMacroses(efGen->m_ShaderGenParams, Parser, nMaskGen);

  pBin->Lock();
  Parser.Preprocess(0, pBin->m_Tokens, &pBin->m_Table);
  ef->m_CRC32 = pBin->m_CRC32;
  ef->m_ModifTime = pBin->m_Time;
  pBin->Unlock();

  SParserFrame Frame(0, Parser.m_Tokens.size()-1);
  Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(static)
    FX_TOKEN(float)
    FX_TOKEN(float2)
    FX_TOKEN(float3)
    FX_TOKEN(float4)
    FX_TOKEN(float2x4)
    FX_TOKEN(float3x4)
    FX_TOKEN(float4x4)
    FX_TOKEN(bool)
    FX_TOKEN(int)
    FX_TOKEN(struct)
    FX_TOKEN(sampler1D)
    FX_TOKEN(sampler2D)
    FX_TOKEN(sampler3D)
    FX_TOKEN(samplerCUBE)
    FX_TOKEN(technique)
    FX_TOKEN(SamplerState)
    FX_TOKEN(SamplerComparisonState)
    FX_TOKEN(Texture2D)
    FX_TOKEN(Texture2DArray)
    FX_TOKEN(Texture2DMS)
  FX_END_TOKENS

  DynArray<SShaderTechParseParams> techParams;
  CCryName techStart[2];
  assert (ef->m_HWTechniques.Num() == 0);
  int nInd = 0;

  while (Parser.ParseObject(sCommands))
  {
    EToken eT = Parser.GetToken();
    SCodeFragment Fr;
    switch (eT)
    {
      case eT_struct:
      case eT_SamplerState:
      case eT_SamplerComparisonState:
        Fr.m_nFirstToken = Parser.FirstToken();
        Fr.m_nLastToken = Parser.m_CurFrame.m_nCurToken-1;
        Fr.m_dwName = Parser.m_Tokens[Fr.m_nFirstToken+1];
#ifdef _DEBUG
        Fr.m_Name = Parser.GetString(Fr.m_dwName);
#endif
        Fr.m_eType = eFT_Structure;
        Parser.m_CodeFragments.push_back(Fr);
        break;

      case eT_Texture2DMS:
        {
          Fr.m_nFirstToken = Parser.FirstToken();
          Fr.m_nLastToken = Fr.m_nFirstToken+6;
          if (!Parser.m_Assign.IsEmpty())
            Fr.m_nLastToken = Parser.m_Assign.m_nLastToken;
          Fr.m_dwName = Parser.m_Tokens[Fr.m_nFirstToken+6];
          Fr.m_eType = eFT_Sampler;
          Parser.m_CodeFragments.push_back(Fr);

          bRes &= ParseBinFX_Sampler(Parser, Parser.m_Data, Fr.m_dwName, Parser.m_Annotations);
        }
        break;

      case eT_int:
      case eT_bool:
      case eT_float:
      case eT_float2:
      case eT_float3:
      case eT_float4:
      case eT_float2x4:
      case eT_float3x4:
      case eT_float4x4:
        {
          SFXParam Pr;
          Parser.CopyTokens(Parser.m_Name, Pr.m_dwName);
          if (eT == eT_float2x4)
            Pr.m_nParameters = 2;
          else
          if (eT == eT_float3x4)
            Pr.m_nParameters = 3;
          else
          if (eT == eT_float4x4)
            Pr.m_nParameters = 4;
          else
            Pr.m_nParameters = 1;

          if (eT == eT_float || eT == eT_int || eT == eT_bool)
            Pr.m_nComps = 1;
          else
          if (eT == eT_float2)
            Pr.m_nComps = 2;
          else
          if (eT == eT_float3)
            Pr.m_nComps = 3;
          else
          if (eT == eT_float4 || eT == eT_float2x4 || eT == eT_float3x4 || eT == eT_float4x4)
            Pr.m_nComps = 4;

          if (eT == eT_int)
            Pr.m_eType = eType_INT;
          else
          if (eT == eT_bool)
            Pr.m_eType = eType_BOOL;
          else
            Pr.m_eType = eType_FLOAT;

          if (!Parser.m_Assign.IsEmpty() && Parser.GetToken(Parser.m_Assign) == eT_STANDARDSGLOBAL)
          {
            ParseBinFX_Global(Parser, Parser.m_Annotations, NULL, techStart);
          }
          else
          {
            uint32 nTokAssign = 0;
            if (Parser.m_Assign.IsEmpty() && !Parser.m_Value.IsEmpty())
            {
              nTokAssign = Parser.m_Tokens[Parser.m_Value.m_nFirstToken];
              if (nTokAssign == eT_br_cv_1)
                nTokAssign = Parser.m_Tokens[Parser.m_Value.m_nFirstToken+1];
            }
            else
            if (!Parser.m_Assign.IsEmpty())
              nTokAssign = Parser.m_Tokens[Parser.m_Assign.m_nFirstToken];
            if (nTokAssign)
            {
              const char *assign = Parser.GetString(nTokAssign);
              if (!strnicmp(assign, "SK_", 3))
                Pr.m_nCB = CB_SKIN_DATA;
              else
              if (!strnicmp(assign, "SH_", 3))
                Pr.m_nCB = CB_SHAPE_DATA;
              else
              if (!assign[0] || !strnicmp(assign, "PB_", 3))
                Pr.m_nCB = CB_PER_BATCH;
              else
              if (!assign[0] || !strnicmp(assign, "SG_", 3))
                Pr.m_nCB = CB_PER_SHADOWGEN;
              else
              if (!strnicmp(assign, "PI_", 3))
                Pr.m_nCB = CB_PER_INSTANCE;
              else
              if (!strnicmp(assign, "PF_", 3))
                Pr.m_nCB = CB_PER_FRAME;
              else
              if (!strnicmp(assign, "PM_", 3))
              {
  #ifdef USE_PER_MATERIAL_PARAMS
                Pr.m_nCB = CB_PER_MATERIAL;
  #else
                Pr.m_nCB = CB_PER_BATCH;
  #endif
              }
              else
              if (!strnicmp(assign, "PL_", 3))
                Pr.m_nCB = CB_PER_LIGHT;
              else
              if (!strnicmp(assign, "register", 8))
                Pr.m_nCB = CB_PER_BATCH;
              else
                Pr.m_nCB = CB_PER_BATCH;
            }
            else
            if (CParserBin::m_bD3D10)
            {
              uint nTokName = Parser.GetToken(Parser.m_Name);
              if (nTokName == eT__g_SkinQuat)
                Pr.m_nCB = CB_SKIN_DATA;
              else
              if (nTokName == eT__g_ShapeDeformationData)
                Pr.m_nCB = CB_SHAPE_DATA;
              else
              {
                const char *name = Parser.GetString(nTokName);
                if (!strncmp(name, "PI_", 3))
                  Pr.m_nCB = CB_PER_INSTANCE;
                else  
                  Pr.m_nCB = CB_PER_BATCH;
              }
            }

            Pr.PostLoad(Parser, Parser.m_Name, Parser.m_Annotations, Parser.m_Value, Parser.m_Assign);

            EParamType eType;
            string szReg = Pr.GetValueForName("register", eType);
            if (!szReg.empty())
            {
              assert(szReg[0] == 'c');
              Pr.m_nRegister[eHWSC_Vertex] = atoi(&szReg[1]);
              Pr.m_nRegister[eHWSC_Pixel] = Pr.m_nRegister[eHWSC_Vertex];
              if (CParserBin::m_bD3D10)
                Pr.m_nRegister[eHWSC_Geometry] = Pr.m_nRegister[eHWSC_Vertex];
            }
            uint32 prFlags = Pr.GetParamFlags();
#ifdef USE_PER_MATERIAL_PARAMS
            if (prFlags & PF_TWEAKABLE_MASK)
            {
              if (!(prFlags & PF_ALWAYS))
              {
                Pr.m_nCB = CB_PER_MATERIAL;
                assert(prFlags & PF_CUSTOM_BINDED);
              }
            }
#endif
            Parser.m_Parameters.push_back(Pr);
          }
        }
        break;

      case eT_Texture2D:
      case eT_Texture2DArray:
      case eT_sampler1D:
      case eT_sampler2D:
      case eT_sampler3D:
      case eT_samplerCUBE:
        {
          Fr.m_nFirstToken = Parser.FirstToken();
          Fr.m_nLastToken = Fr.m_nFirstToken+1;
          if (!Parser.m_Assign.IsEmpty())
            Fr.m_nLastToken = Parser.m_Assign.m_nLastToken;
          Fr.m_dwName = Parser.m_Tokens[Fr.m_nFirstToken+1];
          Fr.m_eType = eFT_Sampler;
          Parser.m_CodeFragments.push_back(Fr);

          bRes &= ParseBinFX_Sampler(Parser, Parser.m_Data, Fr.m_dwName, Parser.m_Annotations);
        }
        break;

      case eT_technique:
        {
          uint32 nToken = Parser.m_Tokens[Parser.m_Name.m_nFirstToken];
          const char *szName = Parser.GetString(nToken);
          SShaderTechnique *pShTech = ParseBinFX_Technique(Parser, Parser.m_Data, Parser.m_Annotations, techParams, NULL);
          if (szName)
            pShTech->m_Name = szName;
        }
        break;

      default:
        assert(0);
    }
  }

  m_pCEF->mfPostLoadFX(ef, techParams, techStart);

  return bRes;
}

bool CShaderManBin::ParseBinFX_Dummy(SShaderBin *pBin, std::vector<string>& ShaderNames, const char *szName)
{
  bool bRes = true;
  CParserBin Parser(pBin, NULL);

  pBin->Lock();
  Parser.Preprocess(0, pBin->m_Tokens, &pBin->m_Table);
  pBin->Unlock();

  SParserFrame Frame(0, Parser.m_Tokens.size()-1);
  Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(static)
    FX_TOKEN(float)
    FX_TOKEN(float2)
    FX_TOKEN(float3)
    FX_TOKEN(float4)
    FX_TOKEN(float2x4)
    FX_TOKEN(float3x4)
    FX_TOKEN(float4x4)
    FX_TOKEN(bool)
    FX_TOKEN(int)
    FX_TOKEN(struct)
    FX_TOKEN(sampler1D)
    FX_TOKEN(sampler2D)
    FX_TOKEN(sampler3D)
    FX_TOKEN(samplerCUBE)
    FX_TOKEN(technique)
    FX_TOKEN(SamplerState)
    FX_TOKEN(SamplerComparisonState)
    FX_TOKEN(Texture2D)
    FX_TOKEN(Texture2DArray)
    FX_TOKEN(Texture2DMS)
  FX_END_TOKENS

  DynArray<SShaderTechParseParams> techParams;
  CCryName techStart[2];
  bool bPublic = false;
  DynArray<string> PubTechniques;

  while (Parser.ParseObject(sCommands))
  {
    EToken eT = Parser.GetToken();
    SCodeFragment Fr;
    switch (eT)
    {
      case eT_float:
        if (!Parser.m_Assign.IsEmpty() && Parser.GetToken(Parser.m_Assign) == eT_STANDARDSGLOBAL)
        {
          ParseBinFX_Global(Parser, Parser.m_Annotations, &bPublic, techStart);
        }
        break;
      case eT_struct:
      case eT_SamplerState:
      case eT_SamplerComparisonState:
      case eT_int:
      case eT_bool:
      case eT_float2:
      case eT_float3:
      case eT_float4:
      case eT_float2x4:
      case eT_float3x4:
      case eT_float4x4:
      case eT_Texture2D:
      case eT_Texture2DArray:
      case eT_sampler1D:
      case eT_sampler2D:
      case eT_sampler3D:
      case eT_samplerCUBE:
        break;

      case eT_Texture2DMS:
        break;

      case eT_technique:
        {
          uint32 nToken = Parser.m_Tokens[Parser.m_Name.m_nFirstToken];
          bool bPublicTechnique = false;
          SShaderTechnique *pShTech = ParseBinFX_Technique(Parser, Parser.m_Data, Parser.m_Annotations, techParams, &bPublicTechnique);
          if (bPublicTechnique)
          {
            const char *szName = Parser.GetString(nToken);
            PubTechniques.push_back(szName);
          }
        }
        break;

      default:
        assert(0);
    }
  }

  if (bPublic)
    ShaderNames.push_back(szName);
  if (PubTechniques.size())
  {
    uint i;
    for (i=0; i<PubTechniques.size(); i++)
    {
      string str = szName;
      str += ".";
      str += PubTechniques[i];
      ShaderNames.push_back(str);
    }
  }

  return bRes;
}

void CShaderMan::mfPostLoadFX(CShader *ef, DynArray<SShaderTechParseParams>& techParams, CCryName techStart[2])
{
  uint i, j;

  ef->m_HWTechniques.Shrink();

  int nVFormat = VERTEX_FORMAT_P3F;
  assert(techParams.size() == ef->m_HWTechniques.Num());
  for (i=0; i<ef->m_HWTechniques.Num(); i++)
  {
    SShaderTechnique *hw = ef->m_HWTechniques[i];
    SShaderTechParseParams *ps = &techParams[i];
    if (ef->m_pSelectTech)
    {
      if (hw->m_Name == techStart[0])
        ef->m_pSelectTech->m_nTech[0] = i;
      if (hw->m_Name == techStart[1])
        ef->m_pSelectTech->m_nTech[1] = i;
    }
    uint n;
    for (n=0; n<TTYPE_MAX; n++)
    {
      if (ps->techName[n].c_str()[0])
      {
        if (hw->m_Name == ps->techName[n])
          iLog->LogWarning("WARNING: technique '%s' refers to itself as the next technique (ignored)", hw->m_Name.c_str());
        else
        {
          uint j;
          for (j=0; j<ef->m_HWTechniques.Num(); j++)
          {
            SShaderTechnique *hw2 = ef->m_HWTechniques[j];
            if (hw2->m_Name == ps->techName[n])
            {
              hw->m_nTechnique[n] = j;
              break;
            }
          }
          if (j == ef->m_HWTechniques.Num())
            iLog->LogWarning("WARNING: couldn't find technique '%s' in the sequence for technique '%s' (ignored)", ps->techName[n].c_str(), hw->m_Name.c_str());
        }
      }
    }
    if (hw->m_nTechnique[TTYPE_Z] >= 0)
    {
      SShaderTechnique *hw2 = ef->m_HWTechniques[hw->m_nTechnique[TTYPE_Z]];
      if (hw2->m_Passes.Num())
      {
        SShaderPass *pass = &hw2->m_Passes[0];
        if (pass->m_RenderState & GS_DEPTHWRITE)
          hw->m_Flags |= FHF_WASZWRITE;
      }
    }
    bool bTransparent = true;
    for (j=0; j<hw->m_Passes.Num(); j++)
    {
      SShaderPass *ps = &hw->m_Passes[j];
#if !defined(PS3)
      if (CParserBin::m_bD3D10 && ps->m_GShader)
        hw->m_Flags |= FHF_USE_GEOMETRY_SHADER;
#endif
      if (!(ps->m_RenderState & GS_BLEND_MASK))
        bTransparent = false;
      if (ps->m_VShader)
      {
        bool bUseLM = false;
        bool bUseTangs = false;
        bool bUseHWSkin = false;
        bool bUseSH = false;
        int nCurVFormat = ps->m_VShader->mfVertexFormat(bUseTangs, bUseLM, bUseHWSkin, bUseSH);
        if (nCurVFormat >= 0)
          nVFormat = gRenDev->m_RP.m_VFormatsMerge[nVFormat][nCurVFormat];
        if (bUseTangs)
        {
          ps->m_PassFlags |= VSM_TANGENTS;
        }
        if (bUseLM)
          ps->m_PassFlags |= VSM_LMTC;
        if (bUseSH)
          ps->m_PassFlags |= VSM_SH;
        if (bUseHWSkin)
        {
          ps->m_PassFlags |= VSM_HWSKIN;
          ps->m_PassFlags |= VSM_HWSKIN_SHAPEDEFORM;
          ps->m_PassFlags |= VSM_HWSKIN_MORPHTARGET;
        }
      }
    }
    if (bTransparent)
      hw->m_Flags |= FHF_TRANSPARENT;
  }
  if ((ef->m_Flags & EF_ENVLIGHTING) && ef->m_HWTechniques.Num())
  {
    SShaderTechnique *pT = ef->m_HWTechniques[0];
    SHRenderTarget *pRT = new SHRenderTarget;
    pRT->m_pTarget[0] = CTexture::m_Text_RT_LCM;
    pRT->m_eOrder = eRO_PreProcess;
    pT->m_RTargets.AddElem(pRT);
  }
  if (ef->m_pSelectTech)
  {
    if (ef->m_pSelectTech->m_nTech[0]<0 && ef->m_pSelectTech->m_nTech[1]>=0)
      ef->m_pSelectTech->m_nTech[0] = ef->m_pSelectTech->m_nTech[1];
    else
      if (ef->m_pSelectTech->m_nTech[1]<0 && ef->m_pSelectTech->m_nTech[0]>=0)
        ef->m_pSelectTech->m_nTech[1] = ef->m_pSelectTech->m_nTech[0];
  }

  ef->m_VertexFormatId = nVFormat;
}

//===========================================================================================

void STexSampler::Update()
{
  if (m_pAnimInfo && m_pAnimInfo->m_Time && gRenDev->m_bPauseTimer==0)
  {
    assert(gRenDev->m_RP.m_RealTime>=0);
    uint m = (uint)(gRenDev->m_RP.m_RealTime / m_pAnimInfo->m_Time) % m_pAnimInfo->m_NumAnimTexs;
    assert(m<(uint)m_pAnimInfo->m_TexPics.Num());
    m_pTex = m_pAnimInfo->m_TexPics[m];
  }
}

string SFXParam::GetCompName(uint nId)
{
  if (nId < 0 || nId > 3)
    return "";
  char nm[16];
  sprintf(nm, "__%d", nId);
  const char *s = strstr(m_Name.c_str(), nm);
  if (!s)
    return "";
  s += 3;
  int n = 0;
  char ss[128];
  while (s[n])
  {
    if (s[n] <= 0x20)
      break;
    if (s[n] == '_' && s[n+1] == '_')
      break;
    ss[n] = s[n];
    n++;
  }
  ss[n] = 0;

  return ss;
}

string SFXParam::GetParamComp(uint nOffset)
{
  const char *szValue = m_Values.c_str();
  if (!szValue[0])
    return "";
  if (szValue[0] == '{')
    szValue++;
  uint n = 0;
  while (n != nOffset)
  {
    while (szValue[0] != ',' && szValue[0] != ';' && szValue[0] != '}' && szValue[0] != 0) {szValue++;}
    if (szValue[0] == ';' || szValue[0] == '}' || szValue[0] == 0)
      return "";
    szValue++;
    n++;
  }
  while (*szValue == ' ' || *szValue == 8) {szValue++;}
  n = 0;
  while (szValue[n] != ',' && szValue[n] != ';' && szValue[n] != '}' && szValue[n] != 0) {n++;}
  string retStr(szValue, n); 

  return retStr;
}

uint SFXParam::GetComponent(EHWShaderClass eSHClass)
{
  char *src = (char *)m_Annotations.c_str();
  char annot[256];
  int nComp = 0;
  fxFill(&src, annot);
  if (!strncmp(&annot[1], "sregister", 9)) 
  {
    if ((eSHClass == eHWSC_Pixel && annot[0] =='v') || (eSHClass == eHWSC_Vertex && annot[0] =='p'))
    {
      fxFill(&src, annot);
      if (strncmp(&annot[1], "sregister", 9)) 
        return 0;
      if ((eSHClass == eHWSC_Pixel && annot[0] =='v') || (eSHClass == eHWSC_Vertex && annot[0] =='p'))
        return 0;
    }
    char *b = &annot[10];
    SkipCharacters (&b, kWhiteSpace);
    assert(b[0] == '=');
    b++;
    SkipCharacters (&b, kWhiteSpace);
    assert(b[0] == 'c');
    if (b[0] == 'c')
    {
      int nReg = atoi(&b[1]);
      b++;
      while (*b != '.')
      {
        if (*b == 0) // Vector without swizzling
          break;
        b++;
      }
      if (*b == '.')
      {
        b++;
        switch(b[0])
        {
        case 'x':
        case 'r': nComp = 0; break;
        case 'y':
        case 'g': nComp = 1; break;
        case 'z':
        case 'b': nComp = 2; break;
        case 'w':
        case 'a': nComp = 3; break;
        default:
          assert(0);
        }
      }
    }
  }
  return nComp;
}

string SFXParam::GetValueForName(const char *szName, EParamType& eType)
{
  eType = eType_UNKNOWN;
  if (m_Annotations.empty())
    return "";

  char buf[256];
  char tok[128];
  char *szA = (char *)m_Annotations.c_str();
  SkipCharacters(&szA, kWhiteSpace);
  while (true)
  {
    if (!fxFill(&szA, buf))
      break;
    char *b = buf;
    fxFillPr(&b, tok);
    eType = eType_UNKNOWN;
    if (!stricmp(tok, "string"))
      eType = eType_STRING;
    else
    if (!stricmp(tok, "float"))
      eType = eType_FLOAT;
    if (eType != eType_UNKNOWN)
    {
      if (!fxFillPr(&b, tok))
        continue;
      if (stricmp(tok, szName))
        continue;
      SkipCharacters(&b, kWhiteSpace);
      if (b[0] == '=')
      {
        b++;
        if (!fxFillPrC(&b, tok))
          break;
      }
      return tok;
    }
    else
    {
      if (stricmp(tok, szName))
        continue;
      eType = eType_STRING;
      if (!fxFillPr(&b, tok))
        continue;
      if (tok[0] == '=')
      {
        if (!fxFillPr(&b, tok))
          break;
      }
      return tok;
    }
  }

  return "";
}

const char *CShaderMan::mfParseFX_Parameter (const string& script, EParamType eType, const char *szName)
{
  static char sRet[128];
  int nLen = script.length();
  char *pTemp = new char [nLen+1];
  strcpy(pTemp, script.c_str());
  sRet[0] = 0;
  char *buf = pTemp;

  char* name;
  long cmd;
  char *data;

  enum {eString = 1};

  static STokenDesc commands[] =
  {
    {eString, "String"},

    {0,0}
  };

  while ((cmd = shGetObject (&buf, commands, &name, &data)) > 0)
  {
    switch (cmd)
    {
    case eString:
      {
        if (eType != eType_STRING)
          break;
        char *szScr = data;
        if (*szScr == '"')
          szScr++;
        int n = 0;
        while(szScr[n] != 0)
        {
          if (szScr[n] == '"')
            szScr[n] = ' ';
          n++;
        }
        if (!stricmp(szName, name))
        {
          strcpy(sRet, data);
          break;
        }
      }
      break;
    }
  }

  SAFE_DELETE_ARRAY(pTemp);

  if (sRet[0])
    return sRet;
  else
    return NULL;
}

SFXParam *CShaderMan::mfGetFXParameter(DynArray<SFXParam>& Params, const char *param)
{
  uint j;
  for (j=0; j<Params.size(); j++)
  {
    SFXParam *pr = &Params[j];
    char nameParam[256];
    int n = 0;
    const char *szSrc = pr->m_Name.c_str();
    int nParams = 1;
    while (*szSrc != 0)
    {
      if (*szSrc == '[')
      {
        nParams = atoi(&szSrc[1]);
        break;
      }
      nameParam[n++] = *szSrc;
      szSrc++;
    }
    nameParam[n] = 0;
    if (!stricmp(nameParam, param))
      return pr;
  }
  return NULL;
}

// We have to parse part of the shader to enumerate public techniques
bool CShaderMan::mfAddFXShaderNames(const char *szName, std::vector<string>& ShaderNames, bool bUpdateCRC)
{
  bool bRes = true;
  SShaderBin *pBin = m_Bin.GetBinShader(szName, false);
  if (!pBin)
    return false;
  if (bUpdateCRC)
    pBin->UpdateCRC(true);
  bRes &= m_Bin.ParseBinFX_Dummy(pBin, ShaderNames, szName);

  return bRes;
}

CTexture *CShaderMan::mfParseFXTechnique_LoadShaderTexture (STexSampler *smp, SShaderPass *pShPass, CShader *ef, int nIndex, byte ColorOp, byte AlphaOp, byte ColorArg, byte AlphaArg)
{
  CTexture *tp = NULL;
  short nFlags = 0;
  const char *szName = smp->m_Texture.c_str();
  if (!szName || !szName[0]) // Sampler without texture specified
    return NULL;
  if (szName[0] == '$')
    tp = mfCheckTemplateTexName(szName, (ETEX_Type)smp->m_eTexType, nFlags);
  if (!tp)
  {
    tp = mfTryToLoadTexture(szName, 0, smp->m_eTexType, -1, -1);
    if (tp && tp->IsTextureLoaded())
      mfCheckAnimatedSequence(smp, tp);
  }

  return tp;
}
