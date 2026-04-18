/*=============================================================================
  Texture.cpp : Common texture manager implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include <ImageExtensionHelper.h>
#include "Image/CImage.h"
#include "IDirectBee.h"			// connection to D3D9 wrapper
#include <IFlashPlayer.h>
#include <IVideoPlayer.h>
#include "I3DEngine.h"
#include "StringUtils.h"								// stristr()

#include "../3Dc/CompressorLib.h"

byte *CTexture::m_pLoadData;
int CTexture::m_nLoadOffset;

CTexture *CTexture::m_pCurrent;
string CTexture::m_GlobalTextureFilter;
int CTexture::m_nGlobalDefState;
STexState CTexture::m_sDefState;
STexState CTexture::m_sGlobalDefState;
STexStageInfo CTexture::m_TexStages[MAX_TMU];
int CTexture::m_TexStateIDs[MAX_TMU];
int CTexture::m_CurStage;
int CTexture::m_nStreamed;
int CTexture::m_nStreamingMode;
int CTexture::m_nCurStages;
int CTexture::m_nMaxtextureSize;
bool CTexture::m_bPrecachePhase;
bool CTexture::m_bStreamingShow;

int CTexture::m_nCurState = -1;
std::vector<STexState> CTexture::m_TexStates;

TArray<STexGrid> CTexture::m_TGrids;

// ==============================================================================
CTexture *CTexture::m_Text_NoTexture;
CTexture *CTexture::m_Text_White;
CTexture *CTexture::m_Text_Gray;
CTexture *CTexture::m_Text_Black;
CTexture *CTexture::m_Text_FlatBump;
CTexture *CTexture::m_Text_PaletteDebug;
CTexture *CTexture::m_Text_IconShaderCompiling;
CTexture *CTexture::m_Text_IconStreaming;
CTexture *CTexture::m_Text_IconStreamingTerrainTexture;
CTexture *CTexture::m_Text_MipColors_Diffuse;
CTexture *CTexture::m_Text_MipColors_Bump;
CTexture *CTexture::m_Text_ScreenNoiseMap;
CTexture *CTexture::m_Text_Noise2DMap;
CTexture *CTexture::m_Text_NoiseBump2DMap;
CTexture *CTexture::m_Text_NoiseVolumeMap;
CTexture *CTexture::m_Text_VectorNoiseVolMap;
CTexture *CTexture::m_Text_NormalizationCubeMap;
CTexture *CTexture::m_Text_LightCMap;
CTexture *CTexture::m_Text_FromRE[8];
CTexture *CTexture::m_Text_ShadowID[8];
CTexture *CTexture::m_Text_FromRE_FromContainer[2];
CTexture *CTexture::m_Text_FromObj[2];
CTexture *CTexture::m_Text_RT_2D;
CTexture *CTexture::m_Text_RT_CM;
CTexture *CTexture::m_Text_RT_LCM;
CTexture *CTexture::m_Text_FromLight;
CTexture *CTexture::m_Text_Flare;
CTexture *CTexture::m_Text_16_PointsOnSphere;
CTexture *CTexture::m_Text_ScreenMap;
CTexture *CTexture::m_Text_ScreenShadowMap[MAX_REND_LIGHT_GROUPS];
CTexture *CTexture::m_Tex_CurrentScreenShadowMap[MAX_REND_LIGHT_GROUPS];
CTexture *CTexture::m_Text_AOTarget;

// Post-process related textures
CTexture *CTexture::m_Text_BackBuffer;
CTexture *CTexture::m_Text_BackBufferScaled[3];
CTexture *CTexture::m_Text_EffectsAccum;
CTexture *CTexture::m_Text_BackBufferLum[2];
CTexture *CTexture::m_Text_BackBufferAvgCol[2];
CTexture *CTexture::m_Text_Glow;

CTexture *CTexture::m_Text_WaterOcean;
CTexture *CTexture::m_Text_WaterVolumeTemp;
CTexture *CTexture::m_Text_WaterVolumeDDN;
CTexture *CTexture::m_Text_WaterPuddles[2];
CTexture *CTexture::m_Text_WaterPuddlesDDN;

CTexture *CTexture::m_Text_ScatterLayer;
CTexture *CTexture::m_Text_TranslucenceLayer;

CTexture *CTexture::m_Text_RT_ShadowPool;
CTexture *CTexture::m_Text_RT_NULL;
CTexture *CTexture::m_Text_TerrainLM;
CTexture *CTexture::m_Text_CloudsLM;

CTexture *CTexture::m_Text_LightInfo[4];
CTexture *CTexture::m_Text_SceneTarget;
CTexture *CTexture::m_Text_SceneTargetFSAA;  
CTexture *CTexture::m_Text_ZTarget;
CTexture *CTexture::m_Text_ZTargetMS;
CTexture *CTexture::m_Text_ZTargetScaled;
CTexture *CTexture::m_Text_HDRTarget;
CTexture *CTexture::m_Text_HDRTargetScaled[3];
CTexture *CTexture::m_Text_HDRBrightPass[2];
CTexture *CTexture::m_Text_HDRAdaptedLuminanceCur[8];
int CTexture::m_nCurLumTexture;
CTexture *CTexture::m_pCurLumTexture;
CTexture *CTexture::m_pIssueLumTexture;
CTexture *CTexture::m_Text_HDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
CTexture *CTexture::m_SkyDomeTextureMie;
CTexture *CTexture::m_SkyDomeTextureRayleigh;
CTexture *CTexture::m_Text_FromSF[NUM_SCALEFORM_TEXTURES];
CTexture *CTexture::m_Text_VolObj_Density;
CTexture *CTexture::m_Text_VolObj_Shadow;

SEnvTexture *CTexture::m_pCurEnvTexture;

SEnvTexture CTexture::m_EnvLCMaps[MAX_ENVLIGHTCUBEMAPS];
SEnvTexture CTexture::m_EnvCMaps[MAX_ENVCUBEMAPS];
SEnvTexture CTexture::m_EnvTexts[MAX_ENVTEXTURES];

TArray<SEnvTexture> CTexture::m_CustomRT_CM;
TArray<SEnvTexture> CTexture::m_CustomRT_2D;

TArray<STexPool *> CTexture::m_TexPools;
STexPoolItem CTexture::m_FreeTexPoolItems;

CTexture CTexture::m_Templates[EFTT_MAX];

CTexture *CTexture::m_pBackBuffer;
CTexture CTexture::m_FrontTexture[2];

int CTexture::m_nCurTexResolution;
int CTexture::m_nCurTexBumpResolution;

#if defined(PS3)
	ETEX_Format CTexture::m_eTFZ = eTF_A16B16G16R16F; 
	//ETEX_Format CTexture::m_eTFZ = eTF_A8R8G8B8;
#else
  ETEX_Format CTexture::m_eTFZ = eTF_R32F; 
	//ETEX_Format CTexture::m_eTFZ = eTF_G16R16F; 
#endif
//============================================================

CTexture::~CTexture()
{
  ReleaseDeviceTexture(false);

  if (m_pPoolItem)
    m_pPoolItem->LinkFree(&CTexture::m_FreeTexPoolItems);
  StreamRemoveFromPool();
  Unlink();
  int i;
  for (i=0; i<MAX_TMU; i++)
  {
    if (m_TexStages[i].m_Texture == this)
      m_TexStages[i].m_Texture = NULL;
  }
  if (m_bStreamRequested)
  {
    for (i=0; i<m_Requested.Num(); i++)
    {
      if (m_Requested[i] == this)
      {
        m_Requested[i] = NULL;
        break;
      }
    }
  }
}

CCryName CTexture::mfGetClassName()
{
  return m_sClassName;
}

CCryName CTexture::GenName(const char *name, uint nFlags, float fAmount1, float fAmount2)
{
  string strName = name;

  if (!(nFlags & FT_DONT_GENNAME))
	{
	  string Name;
 
		strName += Name.Format("(%x)", nFlags & FT_AFFECT_INSTANCE);

		if (CRenderer::CV_r_texbumpheightmap)		// support for legacy feature (bump2normal)
	    strName += Name.Format("(%.2f,%.2f)", fAmount1, fAmount2);
	}

  return CCryName(strName.c_str());
}

CTexture *CTexture::GetByID(int nID)
{
  CTexture *pTex = NULL;

  CCryName className = mfGetClassName();

  CBaseResource *pBR = CBaseResource::GetResource(className, nID, false);
  if (!pBR)
    return m_Text_NoTexture;
  pTex = (CTexture *)pBR;
  return pTex;
}

CTexture *CTexture::GetByName(const char *szName)
{
  CTexture *pTex = NULL;

  CCryName Name = GenName(szName, FT_DONT_GENNAME);
  CCryName className = mfGetClassName();

  CBaseResource *pBR = CBaseResource::GetResource(className, Name, false);
  if (!pBR)
    return NULL;
  pTex = (CTexture *)pBR;
  return pTex;
}

CTexture *CTexture::NewTexture(const char *name, uint nFlags, ETEX_Format eTFDst, bool& bFound, float fAmount1, float fAmount2, int8 nPriority)
{
  CTexture *pTex = NULL;

  CCryName Name = GenName(name, nFlags, fAmount1, fAmount2);
  CCryName className = mfGetClassName();

  CBaseResource *pBR = CBaseResource::GetResource(className, Name, false);
  if (!pBR)
  {
    pTex = new CTexture;
    pTex->Register(className, Name);
    bFound = false;
    pTex->m_nFlags = nFlags;
    pTex->m_eTFDst = eTFDst;
    pTex->m_nPriority = __max(nPriority, pTex->m_nPriority);
  }
  else
  {
    pTex = (CTexture *)pBR;
    pTex->AddRef();
    bFound = true;
    pTex->m_nPriority = __max(nPriority, pTex->m_nPriority);
  }

  return pTex;
}

void CTexture::PostCreate()
{
  int nLogWidth = 1<<LogBaseTwo(m_nWidth);
  int nLogHeight = 1<<LogBaseTwo(m_nHeight);
  if (nLogWidth == m_nWidth && nLogHeight == m_nHeight)
    m_bIsPowerByTwo = true;
  else
    m_bIsPowerByTwo = false;

  m_nSize = CTexture::TextureDataSize(m_nWidth, m_nHeight, m_nDepth, m_nMips, m_eTFDst);

  if (m_eTT == eTT_Cube)
    m_nSize *= 6;
  m_nMips3DC = m_nMips;

#if defined (DIRECT3D10)
  m_nSize *= m_nArraySize;
#else
  if (m_eTFDst == eTF_3DC)
  {
    int nMips = m_nMips;
    int wdt = m_nWidth;
    int hgt = m_nHeight;
    int wd = wdt;
    int hg = hgt;
    if (wd < 4 || hg < 4)
    {
      iLog->Log("Error: 3DC texture '%s' couldn't be streamed due to small size (%dx%d) - skipped", GetName(), wd, hg);
    }
    else
    {
      int nM = 0;
      while (wdt && hgt)
      {
        if (!wdt) wdt = 1;
        if (!hgt) hgt = 1;
        if (wdt >= 4 && hgt >= 4)
          nM++;
        wdt >>= 1;
        hgt >>= 1;
      }
      m_nMips3DC = nM;
    }
  }
#endif
}

CTexture *CTexture::CreateTextureObject(const char *name, uint nWidth, uint nHeight, int nDepth, ETEX_Type eTT, uint nFlags, ETEX_Format eTF, int nCustomID, int8 nPriority)
{
  bool bFound = false;

  CTexture *pTex = NewTexture(name, nFlags, eTF, bFound, -1, -1, nPriority);
  if (bFound)
  {
    if (!pTex->m_nWidth)
    {
      pTex->m_nWidth = nWidth;
      pTex->m_nSrcWidth = nWidth;
    }
    if (!pTex->m_nHeight)
    {
      pTex->m_nHeight = nHeight;
      pTex->m_nSrcHeight = nHeight;
    }
		pTex->m_nFlags |= nFlags & (FT_DONT_RELEASE | FT_USAGE_RENDERTARGET | FT_DONT_ANISO);

    return pTex;
  }
  pTex->m_nDepth = nDepth;
  pTex->m_nWidth = nWidth;
  pTex->m_nHeight = nHeight;
  pTex->m_nSrcWidth = nWidth;
  pTex->m_nSrcHeight = nHeight;
  pTex->m_eTT = eTT;
  pTex->m_eTFDst = eTF;
  pTex->m_nCustomID = nCustomID;
  pTex->m_SrcName = name;

  return pTex;
}

void CTexture::GetMemoryUsage( ICrySizer *pSizer ) 
{ 
	pSizer->AddObject((void *)this,GetSize());
}


CTexture *CTexture::CreateTextureArray(const char *name, uint nWidth, uint nHeight, uint nArraySize, uint nFlags, ETEX_Format eTF, int nCustomID, int8 nPriority)
{
#if defined (DIRECT3D10)
  CTexture *pTex = CreateTextureObject(name, nWidth, nHeight, 1, eTT_2D, nFlags , eTF, nCustomID, nPriority);
  pTex->m_nWidth = nWidth;
  pTex->m_nHeight = nHeight;
  pTex->m_nArraySize = nArraySize;

#ifdef WIN32
  if(CRenderer::m_pDirectBee)
    CRenderer::m_pDirectBee->PushName(name);
#endif

  //FIX: replace CreateRenderTarget call by Create2DTexture
  bool bRes = pTex->CreateRenderTarget(eTF);
  if (!bRes)
    pTex->m_nFlags |= FT_FAILED;
  pTex->PostCreate();

  // 	through NVAPI we tell driver not to sync
  if(bRes)
    if(nFlags&FT_DONTSYNCMULTIGPU)
    {
      gRenDev->Hint_DontSync(*pTex);	
    }

#ifdef WIN32
    if(CRenderer::m_pDirectBee)
      CRenderer::m_pDirectBee->PushName();
#endif

    return pTex;
#else
    return NULL;
#endif
}


CTexture* CTexture::CreateRenderTarget(const char *name, uint nWidth, uint nHeight, ETEX_Type eTT, uint nFlags, ETEX_Format eTF, int nCustomID, int8 nPriority)
{
  CTexture *pTex = CreateTextureObject(name, nWidth, nHeight, 1, eTT, nFlags | FT_USAGE_RENDERTARGET, eTF, nCustomID, nPriority);
	pTex->m_nWidth = nWidth;
	pTex->m_nHeight = nHeight;

#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName(name);
#endif

	bool bRes = pTex->CreateRenderTarget(eTF);
  if (!bRes)
    pTex->m_nFlags |= FT_FAILED;
  pTex->PostCreate();

	// 	through NVAPI we tell driver not to sync
	if(bRes)
	if(nFlags&FT_DONTSYNCMULTIGPU)
	{
		gRenDev->Hint_DontSync(*pTex);	
	}

#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName();
#endif

  return pTex;
}

bool CTexture::Create2DTexture(int nWidth, int nHeight, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst)
{
  if (nMips <= 0)
    nMips = CTexture::CalcNumMips(nWidth, nHeight);
  m_eTFSrc = eTFSrc;
  m_nMips = nMips;

  STexData Data[6];
  Data[0].m_eTF = eTFSrc;
  Data[0].m_nDepth = 1;
  Data[0].m_nWidth = nWidth;
  Data[0].m_nHeight = nHeight;
  Data[0].m_nMips = 1;
  Data[0].m_nSize = CTexture::TextureDataSize(nWidth, nHeight, 1, 1, eTFSrc);
  Data[0].m_pData = pData;

  bool bRes = CreateTexture(Data);
  if (!bRes)
    m_nFlags |= FT_FAILED;

  PostCreate();

  return bRes;
}

CTexture *CTexture::Create2DTexture(const char *szName, int nWidth, int nHeight, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst, int8 nPriority)
{
#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName(szName);
#endif

  CTexture *pTex = CreateTextureObject(szName, nWidth, nHeight, 1, eTT_2D, nFlags, eTFDst, -1, nPriority);
  bool bFound = false;

  pTex->Create2DTexture(nWidth, nHeight, nMips, nFlags, pData, eTFSrc, eTFDst);

#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName();
#endif

	return pTex;
}

bool CTexture::Create3DTexture(int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst)
{
  //if (nMips <= 0)
  //  nMips = CTexture::CalcNumMips(nWidth, nHeight);
  m_eTFSrc = eTFSrc;
  m_nMips = nMips;

  STexData Data[6];
  Data[0].m_eTF = eTFSrc;
  Data[0].m_nWidth = nWidth;
  Data[0].m_nHeight = nHeight;
	Data[0].m_nDepth = nDepth;
  Data[0].m_nMips = 1;
  Data[0].m_nSize = CTexture::TextureDataSize(nWidth, nHeight, nDepth, 1, eTFSrc);
  Data[0].m_pData = pData;

  bool bRes = CreateTexture(Data);
  if (!bRes)
    m_nFlags |= FT_FAILED;

  PostCreate();

  return bRes;
}

CTexture *CTexture::Create3DTexture(const char *szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst, int8 nPriority)
{
#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName(szName);
#endif

  CTexture *pTex = CreateTextureObject(szName, nWidth, nHeight, nDepth, eTT_3D, nFlags, eTFDst, -1, nPriority);
  bool bFound = false;

  pTex->Create3DTexture(nWidth, nHeight, nDepth, nMips, nFlags, pData, eTFSrc, eTFDst);

#ifdef WIN32
	if(CRenderer::m_pDirectBee)
		CRenderer::m_pDirectBee->PushName();
#endif

	return pTex;
}

bool CTexture::Reload(uint nFlags)
{
  byte *pData[6];
  int i;
  bool bOK = false;

  if (m_bStream)
  {
    ReleaseDeviceTexture(false);
    return ToggleStreaming(true);
  }

  for (i=0; i<6; i++)
  {
    pData[i] = 0;
  }
  if (m_nFlags & FT_FROMIMAGE)
  {
    assert(!(m_nFlags & FT_USAGE_RENDERTARGET));
    bOK = LoadFromImage(m_SrcName.c_str(), true, eTF_Unknown);		// true=reloading
    if (!bOK)
      SetNoTexture();
  }
  else
  if (m_nFlags & (FT_USAGE_RENDERTARGET|FT_USAGE_DYNAMIC))
  {
    bOK = CreateDeviceTexture(pData);
    assert(bOK);
  }
  PostCreate();

  return bOK;
}

CTexture *CTexture::ForName(const char *name, uint nFlags, ETEX_Format eTFDst, float fAmount1, float fAmount2, int8 nPriority)
{
  bool bFound = false;

  CTexture *pTex = NewTexture(name, nFlags, eTFDst, bFound, fAmount1, fAmount2, nPriority);
  if (bFound || name[0] == '$')
	{
		if(!bFound)
			pTex->m_SrcName = name;

    return pTex;
	}
  pTex->m_SrcName = name;

  if (CTexture::m_bPrecachePhase && !(nFlags & FT_DONT_STREAM))
  {
    pTex->m_eTFDst = eTFDst;
    if (nFlags & FT_TEX_NORMAL_MAP)
      nFlags |= FT_HAS_ATTACHED_ALPHA;
    pTex->m_nFlags = nFlags;
    pTex->m_bStream = true;
    pTex->m_bWasUnloaded = true;
  }
  else
    pTex->Load(eTFDst, fAmount1, fAmount2);

  return pTex;
}

_inline bool CompareTextures(const CTexture *a, const CTexture *b)
{
  return (stricmp(a->GetSourceName(), b->GetSourceName()) < 0);
}

void CTexture::Precache()
{
  LOADING_TIME_PROFILE_SECTION(iSystem);

  if (!m_bPrecachePhase)
    return;
  if (!gRenDev || gRenDev->CheckDeviceLost())
    return;

  CTimeValue t0 = gEnv->pTimer->GetAsyncTime();
  iLog->Log("-- Precaching textures...");
  iLog->UpdateLoadingScreen(0);

  DynArray<CTexture *> Textures;

  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp)
        continue;
      if (tp->m_bStream && tp->m_bWasUnloaded)
        Textures.push_back(tp);
    }
  }

  std::sort(Textures.begin(), Textures.end(), CompareTextures);

  CTexture::m_pLoadData = new byte [TEXPOOL_LOADSIZE];
  for (int i=0; i<Textures.size(); i++)
  {
    CTexture *tp = Textures[i];
    CTexture::m_nLoadOffset = 0;
    tp->Load(tp->m_eTFDst, -1, -1);
    if((i&15)==0)
      iLog->UpdateLoadingScreen(0);
  }
  SAFE_DELETE_ARRAY(CTexture::m_pLoadData);
  CTexture::m_nLoadOffset = 0;

  CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
  float dt = (t1-t0).GetSeconds();
  CryLog( "Precaching textures done in %.2f seconds",dt );

  m_bPrecachePhase = false;
}

bool CTexture::Load(ETEX_Format eTFDst, float fAmount1, float fAmount2)
{
  m_bWasUnloaded = false;
  m_bStream = false;

  bool bFound = LoadFromImage(m_SrcName.c_str(), false, eTFDst, fAmount1, fAmount2);		// false=not reloading
  if (!bFound)
    SetNoTexture();

  m_nFlags |= FT_FROMIMAGE;
  PostCreate();

  return bFound;
}

bool CTexture::SaveCube(string& name)
{
  static char* cubefaces[6] = {"posx","negx","posy","negy","posz","negz"};
  int i;
  char n[256];
  char cube[256];
  const char *ext = fpGetExtension(name.c_str());
  fpStripExtension(name.c_str(), n);
  int len = strlen(n);
  CImageFile* im[6];
  for (i=0; i<6; i++)
  {
    im[i] = NULL;
  }
  if (len > 5)
  {
    for (int i=0; i<6; i++)
    {
      if (!stricmp(&n[len-4], cubefaces[i]))
      {
        if (n[len-5] == '_') 
          len--;
        n[len-4] = 0;
        break;
      }
    }
  }
  sprintf(cube, "%s.dds", n);
  FILE *fp = iSystem->GetIPak()->FOpen(cube, "rb");
  if (fp)
  {
    iSystem->GetIPak()->FClose(fp);
    name = cube;
    return true;
  }
  int nSuccess = 0;
  for (i=0; i<6; i++)
  {
    sprintf(cube, "%s_%s%s", n, cubefaces[i], ext);
    im[i] = CImageFile::mfLoad_file(cube,false,false);			// false = not reloading
    if (!im[i])
      continue;
    if (im[i]->mfGet_error() != eIFE_OK || im[i]->mfGetFormat() == eTF_Unknown)
    {
      delete im[i];
      continue;
    }
    nSuccess++;
  }
  assert(nSuccess == 6);
  if (nSuccess < 6)
    return false;
  TArray<byte> bData;
  int nWidth = im[0]->mfGet_width();
  int nHeight = im[0]->mfGet_height();
  ETEX_Format eTF = im[0]->mfGetFormat();
  int nMips = im[0]->mfGet_numMips();
  if (nMips <= 0)
    nMips = 1;
  for (i=0; i<6; i++)
  {
    byte *pSrc = im[i]->mfGet_image(0);
    assert (nWidth == im[i]->mfGet_width());
    assert (nHeight == im[i]->mfGet_height());
    assert (nWidth == nHeight);
    nWidth = im[i]->mfGet_width();
    nHeight = im[i]->mfGet_height();
    eTF = im[i]->mfGetFormat();
    nMips = im[i]->mfGet_numMips();
    if (nMips <= 0)
      nMips = 1;
    int nSize = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTF);
    if (strcmp(ext, ".dds"))
    {
      nMips = CTexture::CalcNumMips(nWidth, nHeight);
      pSrc = CTexture::Convert(pSrc, nWidth, nHeight, 1, eTF, eTF, nMips, nSize, true);
    }
    bData.Copy(pSrc, nSize);
    if (pSrc != im[i]->mfGet_image(0))
      delete [] pSrc;
    delete im[i];
  }
  sprintf(cube, "%s.dds", n);
  if (::WriteDDS(&bData[0], nWidth, nHeight, 1, cube, eTF, nMips, eTT_Cube))
    name = cube;

  return true;
}

bool CTexture::ToggleStreaming(int bOn)
{
  if (!(m_nFlags & (FT_FROMIMAGE | FT_DONT_RELEASE)))
    return false;
  if (m_nFlags & FT_DONT_STREAM)
    return false;
  if (bOn)
  {
    if (m_bStream)
      return true;
    ReleaseDeviceTexture(false);
    m_bStream = true;
    if (StreamPrepare(true, m_eTFDst))
      return true;
    SAFE_DELETE_ARRAY(m_pFileTexMips);
    m_bStream = false;
    if (m_bNoTexture)
      return true;
  }
  ReleaseDeviceTexture(false);
  bool bRes = Reload(0);
  return bRes;
}

bool CTexture::LoadFromImage(const char *name, const bool bReload, ETEX_Format eTFDst, float fAmount1, float fAmount2)
{
  string nm[6];
  CImageFile* im[6];
  uint i;
  uint nT = 1;

  if (CRenderer::CV_r_texnoload)
  {
    if (SetNoTexture())
      return true;
  }
	m_eTFDst = eTFDst;

  nm[0] = name;
  nm[0].MakeLower();

  // Support for old FC cube-maps textures
  if (strstr(nm[0].c_str(), "_pos") || strstr(nm[0].c_str(), "_neg"))
    SaveCube(nm[0]);

  STexData Data[6];
  string sNew = nm[0].SpanExcluding("+");
  if (sNew.size() != nm[0].size())
  {
    string second = string(&nm[0].c_str()[sNew.size()+1]);
    nm[0] = sNew;
    nm[1] = second;
    nT = 2;
  }
  for (i=0; i<6; i++)
  {
    im[i] = NULL;
  }

  if (nT == 1 && (m_nStreamed & 1) && !(m_nFlags & FT_DONT_STREAM))
  {
    m_bStream = true;
    if (StreamPrepare(bReload, eTFDst))
      return true;
    m_bStream = false;
    if (m_bNoTexture)
      return true;
  }

  for (i=0; i<nT; i++)
  {
    im[i] = CImageFile::mfLoad_file(nm[i].c_str(),bReload,(m_nFlags & FT_ALPHA) ? FIM_ALPHA : 0);
    if (!im[i])
      continue;
    if (im[i]->mfGet_error() != eIFE_OK || im[i]->mfGetFormat() == eTF_Unknown)
    {
      SAFE_DELETE(im[i]);
      continue;
    }
    if (m_nFlags & FT_ALPHA)
    {
      if (!im[i]->mfGet_image(0))
      {
        assert(!m_pDeviceTexture);
        m_pDeviceTexture = m_Text_White->m_pDeviceTexture;
#if defined (DIRECT3D10)
        m_pDeviceShaderResource = m_Text_White->m_pDeviceShaderResource;
#endif
        m_nDefState = m_Text_White->m_nDefState;
        SAFE_DELETE(im[i]);
        m_nFlags = m_Text_White->GetFlags();
        m_eTFDst = m_Text_White->GetDstFormat();
        m_nMips = m_Text_White->GetNumMips();
        m_nWidth = m_Text_White->GetWidth();
        m_nHeight = m_Text_White->GetHeight();
        m_nDepth = 1;
        m_nDefState = m_Text_White->m_nDefState;
        m_bNoTexture = true;
        continue;
      }
    }
		if (im[i]->mfGet_Flags() & FIM_DONTSTREAM)		// propagate flag from image to texture
			m_nFlags|=FT_DONT_STREAM;
		if (im[i]->mfGet_Flags() & FIM_FILESINGLE)		// propagate flag from image to texture
			m_nFlags|=FT_FILESINGLE;
    if (im[i]->mfGet_Flags() & FIM_NORMALMAP)
    {
			if (!(m_nFlags & FT_TEX_NORMAL_MAP) && name && !CryStringUtils::stristr(name, "_ddn"))
			{
				// becomes reported as editor error
				gEnv->pSystem->Warning( VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_FILE|VALIDATOR_FLAG_TEXTURE,
					name,"Not a normal map texture tried to be used as a normal map: %s",name );
			}
    }
		if(!(m_nFlags & FT_ALPHA) && im[i]->mfGetSrcFormat()!=eTF_3DC && CryStringUtils::stristr(name,"_ddn")!=0 && CryStringUtils::stristr(name,"_ddndif")==0)		// improvable code
		{
			// becomes reported as editor error
			gEnv->pSystem->Warning( VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_FILE|VALIDATOR_FLAG_TEXTURE,
				name,"Non 3dc normal map used (wrong on DX10): %s",name );
		}
    if (im[i]->mfGet_Flags() & FIM_NOTSUPPORTS_MIPS)
    {
      if (!(m_nFlags & FT_NOMIPS))
        m_nFlags |= FT_FORCE_MIPS;
    }
    if (im[i]->mfGet_Flags() & FIM_ALPHA)
      m_nFlags |= FT_HAS_ATTACHED_ALPHA;			// if the image has alpha attached we store this in the CTexture
    Data[i].m_nFlags = im[i]->mfGet_Flags();
    Data[i].m_pData = im[i]->mfGet_image(0);
    Data[i].m_nWidth = im[i]->mfGet_width();
    Data[i].m_nHeight = im[i]->mfGet_height();
    Data[i].m_nDepth = im[i]->mfGet_depth();
    Data[i].m_eTF = im[i]->mfGetFormat();
    Data[i].m_nMips = im[i]->mfGet_numMips();
    Data[i].m_fAmount = (i==0) ? fAmount1 : fAmount2;
    Data[i].m_AvgColor = im[i]->mfGet_AvgColor();
    if ((m_nFlags & FT_NOMIPS) || Data[i].m_nMips <= 0)
      Data[i].m_nMips = 1;
    Data[i].m_nSize = CTexture::TextureDataSize(Data[i].m_nWidth, Data[i].m_nHeight, Data[i].m_nDepth, Data[i].m_nMips, Data[i].m_eTF);
    Data[i].m_FileName = im[i]->m_FileName;
    if (!i)
    {
      if (im[i]->mfIs_image(1))
      {
        //return false;

        for (i=1; i<6; i++)
        {
          Data[i].m_nFlags = im[0]->mfGet_Flags();
          Data[i].m_pData = im[0]->mfGet_image(i);
          Data[i].m_nWidth = im[0]->mfGet_width();
          Data[i].m_nHeight = im[0]->mfGet_height();
          Data[i].m_nDepth = im[0]->mfGet_depth();
          Data[i].m_eTF = im[0]->mfGetFormat();
          Data[i].m_nMips = im[0]->mfGet_numMips();
          Data[i].m_AvgColor = im[0]->mfGet_AvgColor();
          if ((m_nFlags & FT_NOMIPS) || Data[i].m_nMips <= 0)
            Data[i].m_nMips = 1;
          Data[i].m_nSize = CTexture::TextureDataSize(Data[i].m_nWidth, Data[i].m_nHeight, Data[i].m_nDepth, Data[i].m_nMips, Data[i].m_eTF);
          Data[i].m_FileName = im[0]->m_FileName;
        }
        break;
      }
    }
  }

  bool bRes = true;

  if (Data[0].m_pData && Data[1].m_pData && !Data[5].m_pData)
  {
    Exchange(Data[0], Data[1]);
  }

  if (im[0])
  {
#ifdef WIN32
		if(CRenderer::m_pDirectBee)
			CRenderer::m_pDirectBee->PushName(PathUtil::GetFile(name));
#endif
		bRes = CreateTexture(Data);
#ifdef WIN32
		if(CRenderer::m_pDirectBee)
			CRenderer::m_pDirectBee->PushName();
#endif
  }
  else
    bRes = false;

  for (i=0; i<nT; i++)
  {
    SAFE_DELETE (im[i]);
    if (Data[i].m_pData && Data[i].m_bReallocated)
    {
      SAFE_DELETE_ARRAY(Data[i].m_pData);
    }
  }

  return bRes;
}

bool CTexture::CreateTexture(STexData Data[6])
{
  m_nSrcWidth = Data[0].m_nWidth;
  m_nSrcHeight = Data[0].m_nHeight;
  m_nSrcDepth = Data[0].m_nDepth;
  m_eTFSrc = Data[0].m_eTF;
  m_AvgColor = Data[0].m_AvgColor;
	m_bUseDecalBorderCol = (Data[0].m_nFlags & FIM_DECAL) != 0;
  
  if (Data[2].m_pData || (m_nFlags & FT_FORCE_CUBEMAP))
    m_eTT = eTT_Cube;
  else
  if (m_nSrcDepth > 1)
  {
    m_eTT = eTT_3D;
    m_nFlags |= FT_DONT_RESIZE;
  }
  else
    m_eTT = eTT_2D;

  if (m_eTFDst == eTF_Unknown)
    m_eTFDst = m_eTFSrc;

  if (m_nFlags & FT_TEX_NORMAL_MAP)
    GenerateNormalMaps(Data);
  ImagePreprocessing(Data);

  if (m_nMaxtextureSize > 0)
  {
    if (m_nWidth > m_nMaxtextureSize || m_nHeight > m_nMaxtextureSize)
      return false;
  }

  byte *pData[6];
  for (uint i=0; i<6; i++)
  {
    pData[i] = Data[i].m_pData;
  }

  bool bRes = CreateDeviceTexture(pData);

  return bRes;
}

void CTexture::GenerateNormalMaps(STexData Data[6])
{
  byte *dst[2];
  int nMips[2];
  int nSizeWithMips[2];
  dst[0] = dst[1] = NULL;
  nMips[0] = nMips[1] = 0;
  nSizeWithMips[0] = nSizeWithMips[1] = 0;

  if (!(Data[0].m_nFlags & (FIM_NORMALMAP | FIM_DSDT)))
    GenerateNormalMap(&Data[0]);
  if (Data[1].m_pData)
  {
    if (!(Data[1].m_nFlags & (FIM_NORMALMAP | FIM_DSDT)))
      GenerateNormalMap(&Data[1]);
  }

  if (Data[1].m_pData)
  {
    m_bStream = false;
    MergeNormalMaps(Data);
  }
}

void CTexture::GenerateNormalMap(STexData *pData)
{
	if(!CRenderer::CV_r_texbumpheightmap)
	{
		GetISystem()->Warning(VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,GetName(),"Generate DDN from BUMP texture failed (feature is no longer supported), artist need to setup TIF file to do this (tex. memory)");
		return;
	}

	byte *dst;
	int i, j;

	GetISystem()->Warning(VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,GetName(),"Generate DDN from BUMP texture (deprecated feature), artist need to setup TIF file to do this (tex. memory)");
	//PS3HACK
  #if defined(PS3)
	  return; //dont wanna implement all possible surface conversion combinations for D3DXLoadSurfaceFromMemory
  #endif
    int nWidth = pData->m_nWidth;
    int nHeight = pData->m_nHeight;
    int nOutSize = 0;
    byte *pGray = Convert(pData->m_pData, nWidth, nHeight, 1, pData->m_eTF, eTF_L8, 1, nOutSize, true);
    assert(pGray);
    if (!pGray)
      return;
    
    bool bInv = false;

    int nMips;
    float fScale = (pData->m_fAmount >= 0) ? pData->m_fAmount/10.0f : 4.0f;
    if (m_nFlags & FT_NOMIPS)
      nMips = 1;
    else
      nMips = CTexture::CalcNumMips(nWidth, nHeight);
    int mx = nWidth-1;
    int my = nHeight-1;
    TArray<Vec4> *Normals = new TArray<Vec4> [nMips];
    int nSize = nWidth * nHeight * 4;
    if (!CRenderer::CV_r_texnormalmaptype)
    {
      Normals[0].Grow(nWidth*nHeight);
      Vec4 *vDst = &Normals[0][0];
      for(j=0; j<nHeight; j++)
      {
        for(i=0; i<nWidth; i++)
        {
          Vec3 vN;
          vN.x = ((float)pGray[j*nWidth+((i-1)&mx)] - (float)pGray[j*nWidth+((i+1)&mx)]) / 255.0f;
          vN.y = ((float)pGray[((j-1)&my)*nWidth+i] - (float)pGray[((j+1)&my)*nWidth+i]) / 255.0f;
          if (bInv)
          {
            vN[0] = -vN[0];
            vN[1] = -vN[1];
          }
          vN.x *= fScale;
          vN.y *= fScale;
          vN.z = 1.0f;
          vN.NormalizeFast();
          vDst->x = vN.x;
          vDst->y = vN.y;
          vDst->z = vN.z;
          vDst->w = (float)pGray[j*nWidth+i];
          vDst++;
        }
      }
    }
    else
    {
      Normals[0].Grow(nWidth*nHeight);
      Vec4 *vDst = &Normals[0][0];
      for(j=0; j<nHeight; j++)
      {
        for(i=0; i<nWidth; i++)
        {
          Vec3 vN1, vN2;
          vN1.x = ((float)pGray[j*nWidth+i] - (float)pGray[j*nWidth+((i+1)&mx)]) / 255.0f;
          vN1.y = ((float)pGray[j*nWidth+i] - (float)pGray[((j+1)&my)*nWidth+i]) / 255.0f;
          if (bInv)
          {
            vN1.x = -vN1.x;
            vN1.y = -vN1.y;
          }
          vN1.x *= fScale;
          vN1.y *= fScale;
          float f = vN1.y;
          vN1.z = 1.0f;
          vN1.NormalizeFast();

          vN2.x = ((float)pGray[((j+1)&my)*nWidth+i] - (float)pGray[((j+1)&my)*nWidth+((i+1)&mx)]) / 255.0f;
          vN2.y = f;
          if (bInv)
          {
            vN2.x = -vN2.x;
            vN2.y = -vN2.y;
          }
          vN2.x *= fScale;
          vN2.z = 1.0f;
          vN2.NormalizeFast();

          Vec3 vN = vN1 + vN2;
          vN.NormalizeFast();
          vDst->x = vN.x;
          vDst->y = vN.y;
          vDst->z = vN.z;
          vDst->w = (float)pGray[j*nWidth+i];
          vDst++;
        }
      }
    }

    // Generate mips
    if (nMips > 1)
      GenerateMipsForNormalmap(Normals, nWidth, nHeight);

  int nS = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTF_A8R8G8B8);
  dst = new byte[nS];
  ConvertFloatNormalMap_To_ARGB8(Normals, nWidth, nHeight, nMips, dst);
  if (pData->m_pData != pGray)
    delete [] pGray;
  pData->m_eTF = eTF_A8R8G8B8;
  m_eTFDst = eTF_A8R8G8B8;
  pData->AssignData(dst);
	pData->m_nSize = nS;
  delete [] Normals;
}

int CTexture::GenerateMipsForNormalmap(TArray<Vec4>* Normals, int nWidth, int nHeight)
{
  int nMips = CTexture::CalcNumMips(nWidth, nHeight);
  int i, j, l;

  // Generate mips
  int reswp = nWidth;
  int reshp = nHeight;
  for (l=1; l<nMips; l++)
  {
    int resw = nWidth  >> l;
    int resh = nHeight >> l;
    if (!resw)
      resw = 1;
    if (!resh)
      resh = 1;
    Normals[l].Grow(resw*resh);
    Vec4 *curr = &Normals[l][0];
    Vec4 *prev = &Normals[l-1][0];
    int wmul = (reswp == 1) ? 1 : 2;
    int hmul = (reshp == 1) ? 1 : 2;
    for (j=0; j<resh; j++)
    {
      for (i=0; i<resw; i++)
      {
        Vec4 avg;
        if (wmul == 1)
        {
          avg = prev[wmul*i+hmul*j*reswp] +
            prev[wmul*i+(hmul*j+1)*reswp];
          avg.w /= 2.0f;
        }
        else
          if (hmul == 1)
          {
            avg = prev[wmul*i+hmul*j*reswp] +
              prev[wmul*i+1+hmul*j*reswp];
            avg.w /= 2.0f;
          }
          else
          {
            avg = prev[wmul*i+hmul*j*reswp] +
              prev[wmul*i+1+hmul*j*reswp] +
              prev[wmul*i+1+(hmul*j+1)*reswp] +
              prev[wmul*i+(hmul*j+1)*reswp];
            avg.w /= 4.0f;
          }
          float fInvDist = 1.0f / sqrtf(avg.x*avg.x + avg.y*avg.y + avg.z*avg.z);
          avg.x *= fInvDist;
          avg.y *= fInvDist;
          avg.z *= fInvDist;
          *curr = avg;
          curr++;
      }
    }
    reswp = resw;
    reshp = resh;
  }
  return nMips;
}

void CTexture::ConvertFloatNormalMap_To_ARGB8(TArray<Vec4>* Normals, int nWidth, int nHeight, int nMips, byte *pDst)
{
  int i, l;

  int n = 0;
  if (nMips <= 0)
    nMips = 1;
  if (m_eTFDst==eTF_V8U8)
  {
    // Signed normal map
    for (l=0; l<nMips; l++)
    {
      int resw = nWidth  >> l;
      int resh = nHeight >> l;
      if (!resw)
        resw = 1;
      if (!resh)
        resh = 1;
      for (i=0; i<resw*resh; i++)
      {
        Vec4 vN = Normals[l][i];
        pDst[n*2+0] = (byte)(vN.x * 127.5f);
        pDst[n*2+1] = (byte)(vN.y * 127.5f);
        n++;
      }
    }
  }
  else
  {
    // Unsigned normal map
    for (l=0; l<nMips; l++)
    {
      int resw = nWidth  >> l;
      int resh = nHeight >> l;
      if (!resw)
        resw = 1;
      if (!resh)
        resh = 1;
      for (i=0; i<resw*resh; i++)
      {
        Vec4 vN = Normals[l][i];
#ifndef XENON
#if defined (DIRECT3D10)
        pDst[n*4+0] = (byte)(vN.x * 127.0f + 128.0f);
        pDst[n*4+1] = (byte)(vN.y * 127.0f + 128.0f);
        pDst[n*4+2] = (byte)(vN.z * 127.0f + 128.0f);
#else
        pDst[n*4+2] = (byte)(vN.x * 127.0f + 128.0f);
        pDst[n*4+1] = (byte)(vN.y * 127.0f + 128.0f);
        pDst[n*4+0] = (byte)(vN.z * 127.0f + 128.0f);
#endif
        pDst[n*4+3] = (byte)vN.w;
#else
        pDst[n*4+1] = (byte)(vN.x * 127.0f + 128.0f);
        pDst[n*4+2] = (byte)(vN.y * 127.0f + 128.0f);
        pDst[n*4+3] = (byte)(vN.z * 127.0f + 128.0f);
        pDst[n*4+0] = (byte)vN.w;
#endif
        n++;
      }
    }
  }
}

void CTexture::MergeNormalMaps(STexData Data[6])
{
	if(!CRenderer::CV_r_texbumpheightmap)
	{
		GetISystem()->Warning(VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,GetName(),"Combining DDN from BUMP texture failed (feature is no longer supported), artist need to setup TIF file to do this (tex. memory)");
		return;
	}	

	int i, j, n;

	GetISystem()->Warning(VALIDATOR_MODULE_RENDERER,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,GetName(),"Combining DDN with BUMP texture (depricated feature), artist need to setup TIF file to do this (tex. memory)");
//PS3HACK
#if defined(PS3)
  return;
#endif
  if (Data[0].m_pData && Data[0].m_eTF != eTF_A8R8G8B8 && Data[0].m_eTF != eTF_X8R8G8B8)
  {
    int nOutSize = 0;
    byte *pDst = Convert(Data[0].m_pData, Data[0].m_nWidth, Data[0].m_nHeight, Data[0].m_nMips, Data[0].m_eTF, eTF_A8R8G8B8, Data[0].m_nMips, nOutSize, true);
    assert(pDst);
    if (pDst)
      Data[0].AssignData(pDst);
	  Data[0].m_nSize = nOutSize;
    Data[0].m_eTF = eTF_A8R8G8B8;
    m_eTFDst = eTF_A8R8G8B8;
  }

  if (Data[1].m_pData && Data[1].m_eTF != eTF_A8R8G8B8 && Data[1].m_eTF != eTF_X8R8G8B8)
  {
    int nOutSize = 0;
    byte *pDst = Convert(Data[1].m_pData, Data[1].m_nWidth, Data[1].m_nHeight, Data[1].m_nMips, Data[1].m_eTF, eTF_A8R8G8B8, Data[1].m_nMips, nOutSize, true);
    assert(pDst);
    if (pDst)
    {
      Data[1].AssignData(pDst);
		  Data[1].m_nSize = nOutSize;
      Data[1].m_eTF = eTF_A8R8G8B8;
    }
  }

  int Width0 = Data[0].m_nWidth;
  int Height0 = Data[0].m_nHeight;
  byte *pSrc0 = Data[0].m_pData;
  int nMips0 = Data[0].m_nMips;

  int Width1 = Data[1].m_nWidth;
  int Height1 = Data[1].m_nHeight;
  byte *pSrc1 = Data[1].m_pData;
  int nMips1 = Data[1].m_nMips;

  assert(pSrc0 && pSrc1);

  n = 0;
  int nIndexNM = 0;

  int nDstMips = CTexture::CalcNumMips(Width0, Height0);
  TArray<Vec4> *Normals = new TArray<Vec4>[nDstMips];
  if (Width0 == Width1 && Height0 == Height1)
  {
    int wdt = Width0;
    int hgt = Height0;
    for (j=0; j<hgt; j++)
    {
      for (i=0; i<wdt; i++)
      {
        Vec3 vN[2];
#ifndef XENON
        vN[0].x = (pSrc0[n*4+2]/255.0f-0.5f)*2.0f;
        vN[0].y = (pSrc0[n*4+1]/255.0f-0.5f)*2.0f;
        vN[0].z = (pSrc0[n*4+0]/255.0f-0.5f)*2.0f;

        vN[1].x = (pSrc1[n*4+2]/255.0f-0.5f)*2.0f;
        vN[1].y = (pSrc1[n*4+1]/255.0f-0.5f)*2.0f;
        vN[1].z = (pSrc1[n*4+0]/255.0f-0.5f)*2.0f;
#else
        vN[0].x = (pSrc0[n*4+1]/255.0f-0.5f)*2.0f;
        vN[0].y = (pSrc0[n*4+2]/255.0f-0.5f)*2.0f;
        vN[0].z = (pSrc0[n*4+3]/255.0f-0.5f)*2.0f;

        vN[1].x = (pSrc1[n*4+1]/255.0f-0.5f)*2.0f;
        vN[1].y = (pSrc1[n*4+2]/255.0f-0.5f)*2.0f;
        vN[1].z = (pSrc1[n*4+3]/255.0f-0.5f)*2.0f;
#endif
        float fLen = min(vN[nIndexNM].GetLength(), 1.0f);

        vN[0].x /= vN[0].z;
        vN[0].y /= vN[0].z;
        vN[0].z = 1.0f;

        vN[1].x /= vN[1].z;
        vN[1].y /= vN[1].z;
        vN[1].z = 1.0f;

        vN[0] = vN[0] + vN[1];
        vN[0].x *= 2.0f;
        vN[0].y *= 2.0f;
        vN[0].NormalizeFast();
        vN[0] *= fLen;
        vN[0].CheckMax(Vec3(-1,-1,-1));
        vN[0].CheckMin(Vec3(1,1,1));

#ifndef XENON
        Vec4 V = Vec4(vN[0], (float)((pSrc0[n*4+3] + pSrc1[n*4+3]) >> 1));
#else
        Vec4 V = Vec4(vN[0], (float)((pSrc0[n*4+0] + pSrc1[n*4+0]) >> 1));
#endif

        Normals[0].AddElem(V);
        n++;
      }
    }
    if (Data[0].m_nMips > 1)
    {
      Data[0].m_nMips = GenerateMipsForNormalmap(Normals, Width0, Height0);
    }
    int nNewSize = CTexture::TextureDataSize(Width0, Height0, 1, Data[0].m_nMips, eTF_A8R8G8B8);
    if (nNewSize > Data[0].m_nSize)
    {
      byte *pDst = new byte [nNewSize];
      Data[0].AssignData(pDst);
		  Data[0].m_nSize = nNewSize;
    }
    ConvertFloatNormalMap_To_ARGB8(Normals, Width0, Height0, Data[0].m_nMips, Data[0].m_pData);
  }
  else
  {
    int wdt = Width0;
    int hgt = Height0;
    if (!wdt)
      wdt = 1;
    if (!hgt)
      hgt = 1;
    int wdt1 = Width1;
    int hgt1 = Height1;
    int mwdt = wdt1-1;
    int mhgt = hgt1-1;
    for (j=0; j<hgt; j++)
    {
      for (i=0; i<wdt; i++)
      {
        Vec3 vN[2];
#ifndef XENON
        vN[0].x = (pSrc0[n*4+2]/255.0f-0.5f)*2.0f;
        vN[0].y = (pSrc0[n*4+1]/255.0f-0.5f)*2.0f;
        vN[0].z = (pSrc0[n*4+0]/255.0f-0.5f)*2.0f;

        vN[1].x = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+2]/255.0f-0.5f)*2.0f;
        vN[1].y = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+1]/255.0f-0.5f)*2.0f;
        vN[1].z = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+0]/255.0f-0.5f)*2.0f;
#else
        vN[0].x = (pSrc0[n*4+1]/255.0f-0.5f)*2.0f;
        vN[0].y = (pSrc0[n*4+2]/255.0f-0.5f)*2.0f;
        vN[0].z = (pSrc0[n*4+3]/255.0f-0.5f)*2.0f;

        vN[1].x = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+1]/255.0f-0.5f)*2.0f;
        vN[1].y = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+2]/255.0f-0.5f)*2.0f;
        vN[1].z = (pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+3]/255.0f-0.5f)*2.0f;
#endif
        float fLen = min(vN[nIndexNM].GetLength(), 1.0f);

        vN[0].x /= vN[0].z;
        vN[0].y /= vN[0].z;
        vN[0].z = 1.0f;

        vN[1].x /= vN[1].z;
        vN[1].y /= vN[1].z;
        vN[1].z = 1.0f;

        vN[0] = vN[0] + vN[1];
        vN[0].x *= 2.0f;
        vN[0].y *= 2.0f;
        vN[0].NormalizeFast();
        vN[0] *= fLen;
        vN[0].CheckMax(Vec3(-1,-1,-1));
        vN[0].CheckMin(Vec3(1,1,1));

#ifndef XENON
        Vec4 V = Vec4(vN[0], (float)((pSrc0[n*4+3] + pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+3]) >> 1));
#else
        Vec4 V = Vec4(vN[0], (float)((pSrc0[n*4+0] + pSrc1[(i&mwdt)*4+(j&mhgt)*wdt1*4+0]) >> 1));
#endif

        Normals[0].AddElem(V);
        n++;
      }
    }
    if (Data[0].m_nMips > 1)
    {
      Data[0].m_nMips = GenerateMipsForNormalmap(Normals, Width0, Height0);
    }
    int nNewSize = CTexture::TextureDataSize(Width0, Height0, 1, Data[0].m_nMips, eTF_A8R8G8B8);
    if (nNewSize > Data[0].m_nSize)
    {
      byte *pDst = new byte [nNewSize];
      Data[0].AssignData(pDst);
		  Data[0].m_nSize = nNewSize;
    }
    ConvertFloatNormalMap_To_ARGB8(Normals, Width0, Height0, Data[0].m_nMips, Data[0].m_pData);
  }
  delete [] Normals;
}

// 1. Resize the texture data if resolution is not supported by the hardware or needed by the spec requirement
// 2. Convert the texture if Dst format is not supported by the hardware or needed by the spec requirement
void CTexture::ImagePreprocessing(STexData Data[6])
{
  int i;
  for (i=0; i<6; i++)
  {
    STexData *pData = &Data[i];
    if (i >= 1 && !Data[5].m_pData)
      continue;
    if (!pData->m_pData)
      continue;
    ETEX_Format eTF = pData->m_eTF;
    int nMips = pData->m_nMips;
    int nSrcWidth = pData->m_nWidth;
    int nSrcHeight = pData->m_nHeight;
    m_nSrcWidth = nSrcWidth;
    m_nSrcHeight = nSrcHeight;
    int nNewWidth = nSrcWidth;
    int nNewHeight = nSrcHeight;
    bool bPowerOfTwo = true;
    int nNWidth = 1<<LogBaseTwo(nSrcWidth);
    int nNHeight = 1<<LogBaseTwo(nSrcHeight);
    bPowerOfTwo = ((nNWidth == nSrcWidth) && (nNHeight == nSrcHeight));
    int nComps = ComponentsForFormat(eTF);
    if (!(m_nStreamed & 1) && !(m_nFlags & FT_DONT_RESIZE))
    {
      int minSize = max(CRenderer::CV_r_texminsize, 16);
      if (m_nFlags & FT_TEX_NORMAL_MAP)
      {
        if (CRenderer::CV_r_texbumpresolution > 0)
        {
          if (nNewWidth >= minSize || nNewHeight >= minSize)
          {
            int nRes = min(CRenderer::CV_r_texbumpresolution, 4);
            int nNWidth = max(nNewWidth>>nRes, 1);
            int nNHeight = max(nNewHeight>>nRes, 1);
						if (m_eTFDst!=eTF_3DC || nMips>1)
						{
							if (m_eTFDst!=eTF_3DC || (nNWidth>=4 && nNHeight>=4))
							{
								nNewWidth  = nNWidth;
								nNewHeight = nNHeight;
							}
						}
          }
        }
      }
      else
      if (m_nFlags & FT_TEX_SKY)
      {
        if (CRenderer::CV_r_texskyresolution > 0)
        {
          if (nNewWidth >= minSize || nNewHeight >= minSize)
          {
            int nRes = min(CRenderer::CV_r_texskyresolution, 4);
            nNewWidth = max(nNewWidth>>nRes, 1);
            nNewHeight = max(nNewHeight>>nRes, 1);
          }
        }
      }
      else
      if (m_nFlags & FT_TEX_LM)
      {
        if (CRenderer::CV_r_texlmresolution > 0)
        {
          if (nNewWidth >= minSize || nNewHeight >= minSize)
          {
            int nRes = min(CRenderer::CV_r_texlmresolution, 4);
            nNewWidth = max(nNewWidth>>nRes, 1);
            nNewHeight = max(nNewHeight>>nRes, 1);
          }
        }
      }
      else
      {
        if (CRenderer::CV_r_texresolution > 0)
        {
          if (nNewWidth >= minSize || nNewHeight >= minSize)
          {
            int nRes = min(CRenderer::CV_r_texresolution, 4);
            nNewWidth = max(nNewWidth>>nRes, 1);
            nNewHeight = max(nNewHeight>>nRes, 1);
          }
        }
      }
      // User limitation check
      if (CRenderer::CV_r_texmaxsize > 1)
      {
        CRenderer::CV_r_texmaxsize = 1<<LogBaseTwo(CRenderer::CV_r_texmaxsize);

        nNewWidth = min(nNewWidth, CRenderer::CV_r_texmaxsize);
        nNewHeight = min(nNewHeight, CRenderer::CV_r_texmaxsize);
      }
    }

    // Hardware limitation check
    int nMaxSize;
    if ((nMaxSize=GetMaxTextureSize()) > 0)
    {
      nNewWidth = min(nNewWidth, nMaxSize);
      nNewHeight = min(nNewHeight, nMaxSize);
    }
    nNewWidth = max(nNewWidth, 1);
    nNewHeight = max(nNewHeight, 1);

    if (!bPowerOfTwo)
    {
      if (IsDXTCompressed(eTF))
        TextureWarning(pData->m_FileName.c_str(),"Error: CTexMan::ImagePreprocessing: Attempt to load of non-power-of-two compressed texture '%s' (%dx%d)", pData->m_FileName.c_str(), nSrcWidth, nSrcHeight);
      else
        TextureWarning(pData->m_FileName.c_str(),"Warning: CTexMan::ImagePreprocessing: Attempt to load of non-power-of-two texture '%s' (%dx%d)", pData->m_FileName.c_str(), nSrcWidth, nSrcHeight);
    }
    if (nNewWidth != nSrcWidth || nNewHeight != nSrcHeight)
    {
      if (IsDXTCompressed(eTF))
      {
        bool bResample = false;
        if (bPowerOfTwo)
        {
          if (nMips > 1)
          {
            int nLodDW = 0;
            int nLodDH = 0;
            int nOffs = 0;
            int blockSize = (eTF == eTF_DXT1) ? 8 : 16;
            int wdt = nSrcWidth;
            int hgt = nSrcHeight;
            int n = 0;
            while (wdt || hgt)
            {
              if (wdt <= 4 || hgt <= 4)
                break;
              if (!wdt)
                wdt = 1;
              if (!hgt)
                hgt = 1;
              if (wdt == nNewWidth)
                nLodDW = n;
              if (hgt == nNewHeight)
                nLodDH = n;
              if (nLodDH && nLodDW)
                break;
              nOffs += ((wdt+3)/4)*((hgt+3)/4)*blockSize;
              wdt >>= 1;
              hgt >>= 1;
              n++;
            }
            if (nLodDH != nLodDW)
            {
              TextureWarning(pData->m_FileName.c_str(),"Error: CTexMan::ImagePreprocessing: Scaling of '%s' compressed texture is dangerous (non-proportional scaling)", pData->m_FileName.c_str());
            }
            else
            if (n)
            {
              byte *dst = pData->m_pData;
              int nSize = pData->m_nSize;
              memmove(dst, &dst[nOffs], nSize-nOffs);
              pData->m_nSize = nSize-nOffs;
              pData->m_nMips = nMips-n;
              pData->m_nWidth = nNewWidth;
              pData->m_nHeight = nNewHeight;
              bResample = true;
            }
            else
            {
              bResample = true;
              nNewWidth = nSrcWidth;
              nNewHeight = nSrcHeight;
            }
          }
          else
          {
            TextureError(pData->m_FileName.c_str(),"Error: CTexMan::ImagePreprocessing: Scaling of '%s' compressed texture is dangerous (only single mip)", pData->m_FileName.c_str());
            bResample = true;
            nNewWidth = nSrcWidth;
            nNewHeight = nSrcHeight;
          }
        }
        if (!bResample)
        {
          if (m_nMaxtextureSize<=0 || (nSrcWidth<m_nMaxtextureSize && nSrcHeight<m_nMaxtextureSize))
          {
            int nOutSize = 0;
            byte *data_ = Convert(pData->m_pData, nSrcWidth, nSrcHeight, nMips, eTF, eTF_A8R8G8B8, nMips, nOutSize, true);
            byte *dst = new byte[nNewWidth*nNewHeight*4];
            byte *src = data_;
            Resample(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
            SAFE_DELETE_ARRAY(data_);
            data_ = Convert(dst, nNewWidth, nNewHeight, nMips, eTF_A8R8G8B8, eTF, nMips, nOutSize, true);
            SAFE_DELETE_ARRAY(dst);
            int DXTSize = TextureDataSize(nNewWidth, nNewHeight, 1, nMips, eTF);
            assert(nOutSize == DXTSize);
            pData->AssignData(data_);
            pData->m_nSize = DXTSize;
            pData->m_nMips = nMips;
            pData->m_nWidth = nNewWidth;
            pData->m_nHeight = nNewHeight;
          }
        }
      }
      else
      {
        if (bPowerOfTwo)
        {
          bool bResampled = false;
          if (nMips > 1)
          {
            int nLodDW = 0;
            int nLodDH = 0;
            int nOffs = 0;
            int wdt = nSrcWidth;
            int hgt = nSrcHeight;
            int n = 0;
            while (wdt || hgt)
            {
              if (!wdt)
                wdt = 1;
              if (!hgt)
                hgt = 1;

              if (wdt == nNewWidth)
                nLodDW = n;
              if (hgt == nNewHeight)
                nLodDH = n;

              if (nLodDH && nLodDW)
                break;
              nOffs += wdt*hgt*nComps;
              wdt >>= 1;
              hgt >>= 1;
              n++;
            }
            if (nLodDH == nLodDW && n)
            {
              byte *dst = pData->m_pData;
              int nSize = pData->m_nSize;
              memmove(dst, &dst[nOffs], nSize-nOffs);
              pData->m_nSize = nSize-nOffs;
              pData->m_nMips = nMips-n;
              pData->m_nWidth = nNewWidth;
              pData->m_nHeight = nNewHeight;
              bResampled = true;
            }
          }
          if (!bResampled)
          {
            if (nComps == 1)
            {
              byte *dst = new byte[nNewWidth*nNewHeight];
              byte *src = pData->m_pData;
              Resample8(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
              pData->AssignData(dst);
              pData->m_nSize = nNewWidth*nNewHeight;
              pData->m_nMips = 1;
              pData->m_nWidth = nNewWidth;
              pData->m_nHeight = nNewHeight;
            }
            else
            {
              byte *dst;
              byte *src = pData->m_pData;
							if( IsFourBit(eTF) )
							{
								dst = new byte[nNewWidth*nNewHeight*2];
								Resample4Bit(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
								pData->AssignData(dst);
								pData->m_nSize = nNewWidth*nNewHeight*2;
							}
							else
							{
								dst = new byte[nNewWidth*nNewHeight*4];
	              Resample(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
								pData->AssignData(dst);
								pData->m_nSize = nNewWidth*nNewHeight*4;
							}
              pData->m_nMips = 1;
              pData->m_nWidth = nNewWidth;
              pData->m_nHeight = nNewHeight;
            }
          }
        }
        else
        {
          if (nMips > 1)
          {
						byte *src = pData->m_pData;
						if( IsFourBit(eTF) )
						{
							byte *dst = new byte[nNewWidth*nNewHeight*2];
							Resample4Bit(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
							pData->AssignData(dst);
							pData->m_nSize = nNewWidth*nNewHeight*2;
						}
						else
						{
							byte *dst = new byte[nNewWidth*nNewHeight*4];
							byte *src1 = NULL;
							if (nComps == 3)
							{
								src1 = new byte[nSrcWidth*nSrcHeight*4];
								for (int i=0; i<nSrcWidth*nSrcHeight; i++)
								{
									src1[i*4+0] = src[i*3+0];
									src1[i*4+1] = src[i*3+1];
									src1[i*4+2] = src[i*3+2];
									src1[i*4+3] = 255;
								}
								src = src1;
							}
							Resample(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
							SAFE_DELETE_ARRAY(src1);
							pData->AssignData(dst);
							pData->m_nSize = nNewWidth*nNewHeight*4;
						}
            pData->m_nMips = 1;
            pData->m_nWidth = nNewWidth;
            pData->m_nHeight = nNewHeight;
          }
          else
          {
						byte *dst;
            byte *src = pData->m_pData;
						if( IsFourBit(eTF) )
						{
							dst = new byte[nNewWidth*nNewHeight*2];
							Resample4Bit(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
							pData->AssignData(dst);
							pData->m_nSize = nNewWidth*nNewHeight*2;
						}
						else
						{
							dst = new byte[nNewWidth*nNewHeight*4];
	            Resample(dst, nNewWidth, nNewHeight, src, nSrcWidth, nSrcHeight);
							pData->AssignData(dst);
							pData->m_nSize = nNewWidth*nNewHeight*4;
						}
            pData->m_nMips = 1;
            pData->m_nWidth = nNewWidth;
            pData->m_nHeight = nNewHeight;
          }
        }
      }
    }
		//PS3HACK
#if defined(PS3)
		if(m_eTFDst == eTF_3DC)
		{
			pData->m_pData = 0;
			pData->m_nSize = 0;
			m_nWidth = 0;
			m_nHeight = 0;
			m_nDepth = 0;
			m_nMips = 0;
			return;
		}
#endif

    ETEX_Format eTFDst = ClosestFormatSupported(m_eTFDst);
    if (eTFDst != m_eTFDst || eTF != eTFDst)
    {
      if (eTFDst == eTF_Unknown && m_eTFDst != eTF_3DC)
      {
#if defined(_XBOX) || defined(PS3)
	      pData->m_pData = 0;
	      pData->m_nSize = 0;
	      m_nWidth = 0;
        m_nHeight = 0;
        m_nDepth = 0;
	      m_nMips = 0;
	      return ;
#endif
        assert(0);
      }
      
//			if (m_eTFDst == eTF_3DC && eTFDst == eTF_Unknown)
//       eTFDst = eTF_A8R8G8B8;
			if (m_eTFDst == eTF_3DC && eTFDst == eTF_Unknown)
				eTFDst = eTF_DXT5;																// compressed normal map xy stored in ba, z reconstructed in shader 

      int nOutSize = 0;
      byte *data_ = Convert(pData->m_pData, nNewWidth, nNewHeight, pData->m_nMips, eTF, eTFDst, pData->m_nMips, nOutSize, true);
      if (data_)
      {
        pData->m_nSize = nOutSize;
        pData->m_eTF = eTFDst;
        m_eTFSrc = eTFDst;
        m_eTFDst = eTFDst;
        pData->AssignData(data_);
      }
    }
  }

//  assert(Data[0].m_pData);
  if (Data[0].m_pData)
  {
    m_nWidth = Data[0].m_nWidth;
    m_nHeight = Data[0].m_nHeight;
    m_nDepth = Data[0].m_nDepth;
    m_nMips = Data[0].m_nMips;
  }
}

int CTexture::ComponentsForFormat(ETEX_Format eTF)
{
  int nBits = BitsPerPixel(eTF);
  if (nBits < 0)
    return nBits;
  return nBits / 8;
}

int CTexture::BitsPerPixel(ETEX_Format eTF)
{
  switch (eTF)
  {
    case eTF_A8R8G8B8:
    case eTF_X8R8G8B8:
    case eTF_X8L8V8U8:
      return 32;
    case eTF_A4R4G4B4:
      return 16;
    case eTF_R8G8B8:
      return 24;
    case eTF_L8V8U8:
      return 24;
    case eTF_V8U8:
      return 16;
    case eTF_R5G6B5:
    case eTF_R5G5B5:
      return 16;
    case eTF_A16B16G16R16:
    case eTF_A16B16G16R16F:
      return 64;
    case eTF_A32B32G32R32F:
      return 128;
    case eTF_G16R16:
    case eTF_G16R16F:
    case eTF_V16U16:
      return 32;
    case eTF_A8:
    case eTF_L8:
      return 8;
    case eTF_A8L8:
      return 16;
    case eTF_R32F:
      return 32;
    case eTF_R16F:
      return 16;
    case eTF_DXT1:
      return 8;
    case eTF_DXT3:
    case eTF_DXT5:
    case eTF_3DC:
      return 16;

    case 	eTF_DF16:
      return 16;
    case 	eTF_DF24:
      return 24;
    case 	eTF_D16:
      return 16;
    case 	eTF_D24S8:
      return 32;
    case 	eTF_D32F:
      return 32;

    case 	eTF_NULL:
      return 8;

    default:
      assert(0);
  }
  return -1;
}

int CTexture::CalcNumMips(int nWidth, int nHeight)
{
  int nMips = 0;
  while (nWidth || nHeight)
  {
    if (!nWidth)   nWidth = 1;
    if (!nHeight)  nHeight = 1;
    nWidth >>= 1;
    nHeight >>= 1;
    nMips++;
  }
  return nMips;
}

int CTexture::TextureDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF)
{
  if (eTF == eTF_Unknown)
    return 0;

  if (nMips <= 0)
    nMips = 0;
  int nSize = 0;
  int nM = 0;
  while (nWidth || nHeight || nDepth)
  {
    if (!nWidth)
      nWidth = 1;
    if (!nHeight)
      nHeight = 1;
    if (!nDepth)
      nDepth = 1;
    nM++;

    int nSingleMipSize;
    if (IsDXTCompressed(eTF))
    {
      int blockSize = (eTF == eTF_DXT1) ? 8 : 16;
      nSingleMipSize = ((nWidth+3)/4)*((nHeight+3)/4)*nDepth*blockSize;
    }
    else
      nSingleMipSize = nWidth * nHeight * nDepth * BitsPerPixel(eTF)/8;
    nSize += nSingleMipSize;

    nWidth >>= 1;
    nHeight >>= 1;
    nDepth >>= 1;
    if (nMips == nM)
      break;
  }
  //assert (nM == nMips);

  return nSize;
}

const char *CTexture::NameForTextureType(ETEX_Type eTT)
{
  char *sETT;
  switch (eTT)
  {
    case eTT_1D:
      sETT = "1D";
      break;
    case eTT_2D:
      sETT = "2D";
      break;
    case eTT_3D:
      sETT = "3D";
      break;
    case eTT_Cube:
      sETT = "Cube";
      break;
    case eTT_AutoCube:
      sETT = "AutoCube";
      break;
    case eTT_Auto2D:
      sETT = "Auto2D";
      break;
    default:
      assert(0);
      sETT = "Unknown";		// for better behaviour in non debug
      break;
  }
  return sETT;
}

const char *CTexture::NameForTextureFormat(ETEX_Format ETF)
{
  char *sETF;
  switch (ETF)
  {
		case eTF_Unknown:
			sETF = "Unknown";
			break;
    case eTF_R8G8B8:
      sETF = "R8G8B8";
      break;
    case eTF_A8R8G8B8:
      sETF = "A8R8G8B8";
      break;
    case eTF_X8R8G8B8:
      sETF = "X8R8G8B8";
      break;
    case eTF_A8:
      sETF = "A8";
      break;
    case eTF_A8L8:
      sETF = "A8L8";
      break;
		case eTF_L8:
			sETF = "L8";
			break;
		case eTF_A4R4G4B4:
			sETF = "A4R4G4B4";
			break;
    case eTF_DXT1:
      sETF = "DXT1";
      break;
    case eTF_DXT3:
      sETF = "DXT3";
      break;
    case eTF_DXT5:
      sETF = "DXT5";
      break;
    case eTF_3DC:
      sETF = "3DC";
      break;
    case eTF_V16U16:
      sETF = "V16U16";
      break;
    case eTF_X8L8V8U8:
      sETF = "X8L8V8U8";
      break;
    case eTF_V8U8:
      sETF = "V8U8";
      break;
    case eTF_A16B16G16R16F:
      sETF = "A16B16G16R16F";
      break;
    case eTF_A16B16G16R16:
      sETF = "A16B16G16R16";
      break;
    case eTF_A32B32G32R32F:
      sETF = "A32B32G32R32F";
      break;
		case eTF_R16F:
			sETF = "R16F";
			break;
    case eTF_R32F:
      sETF = "R32F";
      break;
    case eTF_G16R16:
      sETF = "G16R16";
      break;
    case eTF_G16R16F:
      sETF = "G16R16F";
      break;
    case eTF_DF16:
      sETF = "DF16";
      break;
		case eTF_DF24:
			sETF = "DF24";
			break;
    case eTF_D16:
      sETF = "D16";
      break;
    case eTF_D24S8:
			sETF = "D24S8";
			break;
    case eTF_D32F:
			sETF = "D32F";
			break;

		case eTF_R5G6B5:
			sETF = "R5G6B5";
			break;
		case eTF_R5G5B5:
			sETF = "R5G5B5";
			break;
    default:
      assert(0);
      sETF = "Unknown";		// for better behaviour in non debug
      break;
  }
  return sETF;
}

ETEX_Format CTexture::TextureFormatForName(const char *sETF)
{
  if (!stricmp(sETF, "R8G8B8"))
    return eTF_R8G8B8;
  if (!stricmp(sETF, "A8R8G8B8"))
    return eTF_A8R8G8B8;
  if (!stricmp(sETF, "A8"))
    return eTF_A8;
  if (!stricmp(sETF, "A8L8"))
    return eTF_A8L8;
  if (!stricmp(sETF, "DXT1"))
    return eTF_DXT1;
  if (!stricmp(sETF, "DXT3"))
    return eTF_DXT3;
  if (!stricmp(sETF, "DXT5"))
    return eTF_DXT5;
  if (!stricmp(sETF, "3DC") || !stricmp(sETF, "ATI2"))
    return eTF_3DC;
  if (!stricmp(sETF, "V16U16"))
    return eTF_V16U16;
  if (!stricmp(sETF, "X8L8V8U8"))
    return eTF_X8L8V8U8;
  if (!stricmp(sETF, "V8U8"))
    return eTF_V8U8;
  if (!stricmp(sETF, "DF16"))
    return eTF_DF16;
  if (!stricmp(sETF, "DF24"))
    return eTF_DF24;
  if (!stricmp(sETF, "D16"))
    return eTF_D16;
  if (!stricmp(sETF, "D24S8"))
    return eTF_D24S8; 
  if (!stricmp(sETF, "D32F"))
    return eTF_D32F;

  assert (0);
  return eTF_Unknown;
}

ETEX_Type CTexture::TextureTypeForName(const char *sETT)
{
  if (!stricmp(sETT, "1D"))
    return eTT_1D;
  if (!stricmp(sETT, "2D"))
    return eTT_2D;
  if (!stricmp(sETT, "3D"))
    return eTT_3D;
  if (!stricmp(sETT, "Cube"))
    return eTT_Cube;
  if (!stricmp(sETT, "AutoCube"))
    return eTT_AutoCube;
  if (!stricmp(sETT, "User"))
    return eTT_User;
  assert(0);
  return eTT_2D;
}

void CTexture::Resample4Bit(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight)
{
	int   i, j;
	const ushort *inrow;
	uint  ifrac, fracstep;
//	ushort iData;

	ushort *uout = (ushort *)pDst;
	const ushort *uin = (const ushort *)pSrc;
	fracstep = nSrcWidth*0x10000/nNewWidth;
	if (!(nNewWidth & 3))
	{
		for (i=0 ; i<nNewHeight ; i++, uout+=nNewWidth)
		{
			inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
			ifrac = fracstep >> 1;
			for (j=0; j<nNewWidth; j+=4)
			{
				uout[j] = inrow[ifrac>>16];
				ifrac += fracstep;
				uout[j+1] = inrow[ifrac>>16];
				ifrac += fracstep;
				uout[j+2] = inrow[ifrac>>16];
				ifrac += fracstep;
				uout[j+3] = inrow[ifrac>>16];
				ifrac += fracstep;
			}
		}
	}
	else
	{
		for (i=0; i<nNewHeight; i++, uout+=nNewWidth)
		{
			inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
			ifrac = fracstep >> 1;
			for (j=0; j<nNewWidth; j++)
			{
				uout[j] = inrow[ifrac>>16];
				ifrac += fracstep;
			}
		}
	}
}


void CTexture::Resample(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight)
{
  int   i, j;
  const uint  *inrow;
  uint  ifrac, fracstep;

  uint *uout = (uint *)pDst;
  const uint *uin = (const uint *)pSrc;
  fracstep = nSrcWidth*0x10000/nNewWidth;
  if (!(nNewWidth & 3))
  {
    for (i=0 ; i<nNewHeight ; i++, uout+=nNewWidth)
    {
      inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
      ifrac = fracstep >> 1;
      for (j=0; j<nNewWidth; j+=4)
      {
        uout[j] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+1] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+2] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+3] = inrow[ifrac>>16];
        ifrac += fracstep;
      }
    }
  }
  else
  {
    for (i=0; i<nNewHeight; i++, uout+=nNewWidth)
    {
      inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
      ifrac = fracstep >> 1;
      for (j=0; j<nNewWidth; j++)
      {
        uout[j] = inrow[ifrac>>16];
        ifrac += fracstep;
      }
    }
  }
}

void CTexture::Resample8(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight)
{
  int   i, j;
  const byte  *inrow;
  uint  ifrac, fracstep;

  byte *uout = pDst;
  const byte *uin = pSrc;
  fracstep = nSrcWidth*0x10000/nNewWidth;
  if (!(nNewWidth & 3))
  {
    for (i=0 ; i<nNewHeight ; i++, uout+=nNewWidth)
    {
      inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
      ifrac = fracstep >> 1;
      for (j=0; j<nNewWidth; j+=4)
      {
        uout[j] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+1] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+2] = inrow[ifrac>>16];
        ifrac += fracstep;
        uout[j+3] = inrow[ifrac>>16];
        ifrac += fracstep;
      }
    }
  }
  else
  {
    for (i=0; i<nNewHeight; i++, uout+=nNewWidth)
    {
      inrow = uin + nSrcWidth*(i*nSrcHeight/nNewHeight);
      ifrac = fracstep >> 1;
      for (j=0; j<nNewWidth; j++)
      {
        uout[j] = inrow[ifrac>>16];
        ifrac += fracstep;
      }
    }
  }
}

bool CTexture::Invalidate(int nNewWidth, int nNewHeight, ETEX_Format eTF)
{
  bool bRelease = false;
  if (nNewWidth > 0 && nNewWidth != m_nWidth)
  {
    m_nWidth = nNewWidth;
    bRelease = true;
  }
  if (nNewHeight > 0 && nNewHeight != m_nHeight)
  {
    m_nHeight = nNewHeight;
    bRelease = true;
  }
  if (eTF != eTF_Unknown && eTF != m_eTFDst)
  {
    m_eTFDst = eTF;
    bRelease = true;
  }
  if (!m_pDeviceTexture)
    return false;

  if (bRelease)
    ReleaseDeviceTexture(true);

  return bRelease;
}

byte *CTexture::GetData32(int nSide, int nLevel)
{
  int nPitch;
  byte *pData = LockData(nPitch, nSide, nLevel);
  byte *pDst = NULL;
  if (!pData)
    return NULL;
  if (m_eTFDst != eTF_A8R8G8B8 && m_eTFDst != eTF_X8R8G8B8)
  {
    int nOutSize = 0;
    pDst = Convert(pData, m_nWidth, m_nHeight, 1, m_eTFSrc, eTF_A8R8G8B8, 1, nOutSize, true);
  }
  else
  {
    pDst = new byte [m_nWidth*m_nHeight*4];
    memcpy(pDst, pData, m_nWidth*m_nHeight*4);
  }
  UnlockData(nSide, nLevel);
  return pDst;
}

int CTexture::GetSize()
{
  int nSize = sizeof(CTexture);

  return nSize;
}

void CTexture::Init()
{
  m_nCurTexResolution = CRenderer::CV_r_texresolution;
  m_nCurTexBumpResolution = CRenderer::CV_r_texbumpresolution;

  InitStreaming();
  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
  for (int i=0; i<MAX_TMU; i++)
  {
    m_TexStateIDs[i] = -1;
  }
  m_nTexSizeHistory = 0;
  m_nProcessedTextureID1 = 0;
  m_pProcessedTexture1 = NULL;
  m_nPhaseProcessingTextures = 0;
  m_fAdaptiveStreamDistRatio = 1.0f;

  CTexture::LoadDefaultTextures();
  SDynTexture::Init();
  SDynTexture2::Init(eTP_Clouds);
  SDynTexture2::Init(eTP_Sprites);
}

int __cdecl TexCallback( const VOID* arg1, const VOID* arg2 )
{
  CTexture **pi1 = (CTexture **)arg1;
  CTexture **pi2 = (CTexture **)arg2;
  CTexture *ti1 = *pi1;
  CTexture *ti2 = *pi2;
  if (ti1->GetDeviceDataSize() > ti2->GetDeviceDataSize())
    return -1;
  if (ti1->GetDeviceDataSize() < ti2->GetDeviceDataSize())
    return 1;
  return stricmp(ti1->GetSourceName(), ti2->GetSourceName());
}

void CTexture::Update()
{
  CRenderer *rd = gRenDev;
  int i;
  char buf[256]="";

  AsyncRequestsProcessing();

  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  ResourcesMapItor itor;

  if (m_nStreamingMode != CRenderer::CV_r_texturesstreaming)
  {
    InitStreaming();
    assert(m_nStreamingMode == CRenderer::CV_r_texturesstreaming);
  }

  if (pRL)
  {
		bool bUpdateNonDDNTextures = CRenderer::CV_r_texresolution!=m_nCurTexResolution;
		bool bUpdateDDNTextures = CRenderer::CV_r_texbumpresolution!=m_nCurTexBumpResolution;

    if (bUpdateNonDDNTextures || bUpdateDDNTextures)
    {
      m_nCurTexResolution = CRenderer::CV_r_texresolution;
      m_nCurTexBumpResolution = CRenderer::CV_r_texbumpresolution;

      CHWShader::mfFlushPendedShaders();

      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (!tp || (tp->GetFlags() & (FT_DONT_RELEASE | FT_DONT_RESIZE)))
          continue;
        if (!(tp->GetFlags() & FT_FROMIMAGE))
          continue;
        if (tp->GetTextureType() == eTT_Cube)
        {
          //if (tp->m_CubeSide > 0)
//            continue;
          if(bUpdateNonDDNTextures)
            tp->Reload(tp->m_nFlags);
        }
        else
        if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
        {
          int nFlags = tp->GetFlags();

					if(bUpdateDDNTextures)
            tp->Reload(nFlags);
        }
        else
        {
          if (bUpdateNonDDNTextures)
            tp->Reload(tp->m_nFlags);
        }
      }
    }

    if (CRenderer::CV_r_texlog == 2 || CRenderer::CV_r_texlog == 3 || CRenderer::CV_r_texlog == 4)
    {
      FILE *fp = NULL;
      TArray<CTexture *> Texs;
      int Size = 0;
      int PartSize = 0;
      if (CRenderer::CV_r_texlog == 2 || CRenderer::CV_r_texlog == 3)
      {
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CTexture *tp = (CTexture *)itor->second;
          if (CRenderer::CV_r_texlog == 3 && tp->IsNoTexture())
          {
            Texs.AddElem(tp);
          }
          else
          if (CRenderer::CV_r_texlog == 2 && !tp->IsNoTexture() && (tp->GetFlags() & FT_FROMIMAGE))
          {
            Texs.AddElem(tp);
          }
        }
        if (CRenderer::CV_r_texlog == 3)
				{
					CryLogAlways( "Logging to MissingTextures.txt..." );
          fp = fxopen("MissingTextures.txt", "w");
				}
        else
				{
					CryLogAlways( "Logging to UsedTextures.txt..." );
          fp = fxopen("UsedTextures.txt", "w");
				}
        fprintf(fp, "*** All textures: ***\n");

				if(Texs.Num())
	        qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback);

        for (i=0; i<Texs.Num(); i++)
        {
					int w = Texs[i]->GetWidth();
					int h = Texs[i]->GetHeight();
          fprintf(fp, "%d\t\t%d x %d\t\tType: %s\t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDeviceDataSize(),w,h,Texs[i]->NameForTextureType(Texs[i]->GetTextureType()), Texs[i]->NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
          Size += Texs[i]->GetDataSize();
          PartSize += Texs[i]->GetDeviceDataSize();
        }
        fprintf(fp, "*** Total Size: %d\n\n", Size /*, PartSize, PartSize */);

        Texs.Free();
      }
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (tp->m_nAccessFrameID == rd->m_nFrameUpdateID)
        {
          Texs.AddElem(tp);
        }
      }

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}

			fp = fxopen("UsedTextures_Frame.txt", "w");

      if (fp)
        fprintf(fp, "\n\n*** Textures used in current frame: ***\n");
      else
        rd->TextToScreenColor(4,13, 1,1,0,1, "*** Textures used in current frame: ***");
      int nY = 17;
      
			if(Texs.Num())
				qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback);
      
			Size = 0;
      for (i=0; i<Texs.Num(); i++)
      {
        if (fp)
          fprintf(fp, "%.3fKb\t\tType: %s\t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDeviceDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
        else
        {
          sprintf(buf, "%.3fKb  Type: %s  Format: %s  (%s)", Texs[i]->GetDeviceDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
          rd->TextToScreenColor(4, nY, 0,1,0,1, buf);
          nY += 3;
        }
        PartSize += Texs[i]->GetDeviceDataSize();
        Size += Texs[i]->GetDataSize();
      }
      if (fp)
      {
        fprintf(fp, "*** Total Size: %.3fMb, Device Size: %.3fMb\n\n", Size/(1024.0f*1024.0f), PartSize/(1024.0f*1024.0f));
      }
      else
      {
        sprintf(buf, "*** Total Size: %.3fMb, Device Size: %.3fMb", Size/(1024.0f*1024.0f), PartSize/(1024.0f*1024.0f));
        rd->TextToScreenColor(4,nY+1, 0,1,1,1, buf);
      }

      Texs.Free();
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (tp && !tp->IsNoTexture())
        {
          Texs.AddElem(tp);
        }
      }

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}
			fp = fxopen("UsedTextures_All.txt", "w");

      if (fp)
        fprintf(fp, "\n\n*** All Existing Textures: ***\n");
      else
        rd->TextToScreenColor(4,13, 1,1,0,1, "*** Textures loaded: ***");

			if(Texs.Num())
	      qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback);

			Size = 0;
      for (i=0; i<Texs.Num(); i++)
      {
        if (fp)
				{
					int w = Texs[i]->GetWidth();
					int h = Texs[i]->GetHeight();
          fprintf(fp, "%d\t\t%d x %d\t\t%d mips (%.3fKb)\t\tType: %s \t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDataSize(),w,h,Texs[i]->GetNumMips(), Texs[i]->GetDeviceDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
				}
        else
        {
          sprintf(buf, "%.3fKb  Type: %s  Format: %s  (%s)", Texs[i]->GetDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
          rd->TextToScreenColor(4,nY, 0,1,0,1, buf);
          nY += 3;
        }
        Size += Texs[i]->GetDeviceDataSize();
      }
      if (fp)
      {
        fprintf(fp, "*** Total Size: %.3fMb\n\n", Size/(1024.0f*1024.0f));
      }
      else
      {
        sprintf(buf, "*** Total Size: %.3fMb", Size/(1024.0f*1024.0f));
        rd->TextToScreenColor(4,nY+1, 0,1,1,1, buf);
      }


      Texs.Free();
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (tp && !tp->IsNoTexture() && !tp->IsStreamed())
        {
          Texs.AddElem(tp);
        }
      }
			/*
      if (fp)
        fprintf(fp, "\n\n*** Textures non-streamed: ***\n");
      else
        rd->TextToScreenColor(4,13, 1,1,0,1, "*** Textures non-streamed: ***");

			if(Texs.Num())
	      qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback );

      Size = 0;
      for (i=0; i<Texs.Num(); i++)
      {
        if (fp)
          fprintf(fp, "%.3fKb\t\tType: %s\t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDeviceDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureType(Texs[i]->GetTextureType()), Texs[i]->GetName());
        else
        {
          sprintf(buf, "%.3fKb  Type: %s  Format: %s  (%s)", Texs[i]->GetDeviceDataSize()/1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
          rd->TextToScreenColor(4,nY, 0,1,0,1, buf);
          nY += 3;
        }
        Size += Texs[i]->GetDeviceDataSize();
      }
      if (fp)
      {
        fprintf(fp, "*** Total Size: %.3fMb\n\n", Size/(1024.0f*1024.0f));
        fclose (fp);
      }
      else
      {
        sprintf(buf, "*** Total Size: %.3fMb", Size/(1024.0f*1024.0f));
        rd->TextToScreenColor(4,nY+1, 0,1,1,1, buf);
      }
			*/

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}

      if (CRenderer::CV_r_texlog != 4)
        CRenderer::CV_r_texlog = 0;
    }
    else
    if (CRenderer::CV_r_texlog == 1)
    {
      //char *str = GetTexturesStatusText();

      TArray<CTexture *> Texs;
      //TArray<CTexture *> TexsNM;
      int i;
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (tp && !tp->IsNoTexture())
        {
          Texs.AddElem(tp);
          //if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
          //  TexsNM.AddElem(tp);
        }
      }

			if(Texs.Num())
	      qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback);

			int AllSize = 0;
      int Size = 0;
      int PartSize = 0;
      int NonStrSize = 0;
			int nNoStr = 0;
      int SizeNM = 0;
      int SizeDynCom = 0;
      int SizeDynAtl = 0;
      int PartSizeNM = 0;
      int nNumTex = 0;
			int nNumTexNM = 0;
      int nNumTexDynAtl = 0;
      int nNumTexDynCom = 0;
      for (i=0; i<Texs.Num(); i++)
      {
        if (Texs[i]->GetDeviceTexture() && !(Texs[i]->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)))
        {
          AllSize += Texs[i]->GetDataSize();
          if (!Texs[i]->IsStreamed())
          {
            NonStrSize += Texs[i]->GetDataSize();
            nNoStr++;
          }
        }

        if (Texs[i]->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
        {
          if (Texs[i]->GetFlags() & FT_USAGE_ATLAS)
          {
            ++nNumTexDynAtl;
            SizeDynAtl += Texs[i]->GetDataSize();
          }
          else
          {
            ++nNumTexDynCom;
            SizeDynCom += Texs[i]->GetDataSize();
          }
        }
        else
				if(0 == (Texs[i]->GetFlags() & FT_TEX_NORMAL_MAP))
				{
          if (!Texs[i]->IsUnloaded())
					{
						++nNumTex;
						Size += Texs[i]->GetDataSize();
  					PartSize += Texs[i]->GetDeviceDataSize();
					}
				}
				else
				{
					if (!Texs[i]->IsUnloaded())
					{
						++nNumTexNM;
						SizeNM += Texs[i]->GetDataSize();
						PartSizeNM += Texs[i]->GetDeviceDataSize();
					}
        }
      }

			sprintf(buf, "All texture objects: %d (Size: %.3fMb), NonStreamed: %d (Size: %.3fMb)", Texs.Num(), AllSize/(1024.0f*1024.0f), nNoStr, NonStrSize/(1024.0f*1024.0f));
      rd->TextToScreenColor(4,13, 1,1,0,1, buf);
      sprintf(buf, "All Loaded Texture Maps: %d (All MIPS: %.3fMb, Loaded MIPS: %.3fMb)", nNumTex, Size/(1024.0f*1024.0f), PartSize/(1024.0f*1024.0f));
      rd->TextToScreenColor(4,16, 1,1,0,1, buf);
      sprintf(buf, "All Loaded Normal Maps: %d (All MIPS: %.3fMb, Loaded MIPS: %.3fMb)", nNumTexNM, SizeNM/(1024.0f*1024.0f), PartSizeNM/(1024.0f*1024.0f));
      rd->TextToScreenColor(4,19, 1,1,0,1, buf);
      sprintf(buf, "All Dynamic textures: %d (%.3fMb), %d Atlases (%.3fMb), %d Separared (%.3fMb)", nNumTexDynAtl+nNumTexDynCom, (SizeDynAtl+SizeDynCom)/(1024.0f*1024.0f), nNumTexDynAtl, SizeDynAtl/(1024.0f*1024.0f), nNumTexDynCom, SizeDynCom/(1024.0f*1024.0f));
      rd->TextToScreenColor(4,22, 1,1,0,1, buf);

      Texs.Free();
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (tp && !tp->IsNoTexture() && tp->m_nAccessFrameID == rd->m_nFrameUpdateID)
        {
          Texs.AddElem(tp);
        }
      }

			if(Texs.Num())
	      qsort(&Texs[0], Texs.Num(), sizeof(CTexture *), TexCallback);

			Size = 0;
      SizeDynAtl = 0;
      SizeDynCom = 0;
      PartSize = 0;
      NonStrSize = 0;
      for (i=0; i<Texs.Num(); i++)
      {
        Size += Texs[i]->GetDataSize();
        if (Texs[i]->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
        {
          if (Texs[i]->GetFlags() & FT_USAGE_ATLAS)
            SizeDynAtl += Texs[i]->GetDataSize();
          else
            SizeDynCom += Texs[i]->GetDataSize();
        }
        else
          PartSize += Texs[i]->GetDeviceDataSize();
        if (!Texs[i]->IsStreamed())
          NonStrSize += Texs[i]->GetDataSize();
      }
      sprintf(buf, "Current tex. objects: %d (Size: %.3fMb, Dyn. Atlases: %.3f, Dyn. Separated: %.3f, Loaded: %.3f, NonStreamed: %.3f)", Texs.Num(), Size/(1024.0f*1024.0f), SizeDynAtl/(1024.0f*1024.0f), SizeDynCom/(1024.0f*1024.0f), PartSize/(1024.0f*1024.0f), NonStrSize/(1024.0f*1024.0f));
      rd->TextToScreenColor(4,27, 1,0,0,1, buf);
    }
  }
}

bool CTexture::SetProjector(int nUnit, int nState, int nSamplerSlot)
{
  CRenderer *rd = gRenDev;

  bool bRes = false;
  SLightPass *lp = &rd->m_RP.m_LPasses[rd->m_RP.m_nCurLightPass];
  assert (lp->nLights==1 && (lp->pLights[0]->m_Flags & DLF_PROJECT));
  CDLight *dl = lp->pLights[0];
  if (dl && dl->m_pLightImage!=0)
  {
    bRes = true;
    CTexture *tp = (CTexture *)dl->m_pLightImage;
    if (dl->m_fAnimSpeed)
    {
      int n = 0;
      CTexture *t = tp;
      while (t)
      {
        t = t->m_NextTxt;
        n++;
      }
      if (n > 1)
      {
        int m = (int)(rd->m_RP.m_RealTime / dl->m_fAnimSpeed) % n;
        for (int i=0; i<m; i++)
        {
          tp = tp->m_NextTxt;
        }
      }
    }
    tp->Apply(nUnit, nState, -1, nSamplerSlot);
  }
  return bRes;
}

//=========================================================================

SEnvTexture *CTexture::FindSuitableEnvLCMap(Vec3& Pos, bool bMustExist, int RendFlags, float fDistToCam, CRenderObject *pObj)
{
  float time0 = iTimer->GetAsyncCurTime();

  SEnvTexture *cm = NULL;
  int i;
  CRenderer *rd = gRenDev;
  
  float dist = 999999;
  int firstForUse = -1;
  int firstFree = -1;
  for (i=0; i<MAX_ENVLIGHTCUBEMAPS; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvLCMaps[i];
    Vec3 delta = cur->m_ObjPos - Pos;
    float s = delta.GetLengthSquared();
    if (s < dist)
    {
      dist = s;
      firstForUse = i;
      if (!dist)
        break;
    }
    if (!cur->m_bReady && firstFree < 0)
      firstFree = i;
  }
  if (bMustExist)
  {
    if (firstForUse >= 0)
    {
      rd->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
      return &CTexture::m_EnvLCMaps[firstForUse];
    }
    else
      return NULL;
  }

	float curTime = iTimer->GetCurrTime();
  int nUpdate = -2;
  dist = sqrtf(dist);
  fDistToCam = max(1.0f, fDistToCam);
  if (bMustExist)
    nUpdate = -2;
  else
  if (dist > MAX_ENVLIGHTCUBEMAPSCANDIST_THRESHOLD)
  {
    if (firstFree >= 0)
      nUpdate = firstFree;
    else
      nUpdate = -1;
  }
  else
  {
    float fTimeInterval = max(fDistToCam, 1.0f) * CRenderer::CV_r_envlcmupdateinterval;
    float fDelta = curTime - CTexture::m_EnvLCMaps[firstForUse].m_TimeLastUpdated;
    if (fDelta > fTimeInterval)
      nUpdate = firstForUse;
  }

  if (nUpdate == -2)
  {
    if (!bMustExist)
    {
      for (int i=0; i<6; i++)
      {
        if (CTexture::m_EnvLCMaps[firstForUse].m_nFrameCreated[i]>0)
        {
          //CTexture::GetAverageColor(&CTexture::m_EnvLCMaps[firstForUse], i);
        }
      }
    }

    // No need to update (Up to date)
    rd->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return &CTexture::m_EnvLCMaps[firstForUse];
  }
  if (nUpdate >= 0)
  {
    if (!CTexture::m_EnvLCMaps[nUpdate].m_pTex->m_pTexture || (fDistToCam <= MAX_ENVLIGHTCUBEMAPSCANDIST_UPDATE && gRenDev->m_RP.m_PS.m_fEnvCMapUpdateTime < 0.1f))
    {
      // Reuse old cube-map
      if (!CTexture::m_EnvLCMaps[nUpdate].m_pTex->m_pTexture && pObj)
      {
        for (i=0; i<6; i++)
        {
          CTexture::m_EnvLCMaps[nUpdate].m_EnvColors[i] = pObj->m_II.m_AmbColor;
        }
      }
      int n = nUpdate;
      CTexture::m_EnvLCMaps[n].m_TimeLastUpdated = curTime;
      CTexture::m_EnvLCMaps[n].m_ObjPos = Pos;
      CTexture::ScanEnvironmentCube(&CTexture::m_EnvLCMaps[n], RendFlags, 8, true);
    }
    rd->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return &CTexture::m_EnvLCMaps[nUpdate];
  }

  // Find oldest slot
  dist = 0;
  int nOldest = -1;
  for (i=0; i<MAX_ENVLIGHTCUBEMAPS; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvLCMaps[i];
    if (dist < curTime-cur->m_TimeLastUpdated && !cur->m_bInprogress)
    {
      dist = curTime - cur->m_TimeLastUpdated;
      nOldest = i;
    }
  }
  if (nOldest < 0)
  {
    rd->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return NULL;
  }
  int n = nOldest;
  CTexture::m_EnvLCMaps[n].m_TimeLastUpdated = curTime;
  CTexture::m_EnvLCMaps[n].m_ObjPos = Pos;
  // Fill box colors by nearest cube
  if (firstForUse >= 0)
  {
    for (i=0; i<6; i++)
    {
      CTexture::m_EnvLCMaps[n].m_EnvColors[i] = CTexture::m_EnvLCMaps[firstForUse].m_EnvColors[i];
      CTexture::m_EnvLCMaps[n].m_nFrameCreated[i] = -1;
    }
  }
  // Start with positive X
  CTexture::m_EnvLCMaps[n].m_MaskReady = 0;
  CTexture::ScanEnvironmentCube(&CTexture::m_EnvLCMaps[n], RendFlags, 8, true);

  rd->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
  return &CTexture::m_EnvLCMaps[n];
}

SEnvTexture *CTexture::FindSuitableEnvCMap(Vec3& Pos, bool bMustExist, int RendFlags, float fDistToCam)
{
  float time0 = iTimer->GetAsyncCurTime();

  SEnvTexture *cm = NULL;
  int i;
  
  float dist = 999999;
  int firstForUse = -1;
  int firstFree = -1;
  for (i=0; i<MAX_ENVCUBEMAPS; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvCMaps[i];
    Vec3 delta = cur->m_CamPos - Pos;
    float s = delta.GetLengthSquared();
    if (s < dist)
    {
      dist = s;
      firstForUse = i;
      if (!dist)
        break;
    }
    if (!cur->m_pTex->m_pTexture && firstFree < 0)
      firstFree = i;
  }

	float curTime = iTimer->GetCurrTime();
  int nUpdate = -2;
  float fTimeInterval = fDistToCam * CRenderer::CV_r_envcmupdateinterval + CRenderer::CV_r_envcmupdateinterval*0.5f;
  float fDelta = curTime - CTexture::m_EnvCMaps[firstForUse].m_TimeLastUpdated;
  if (bMustExist)
    nUpdate = -2;
  else
  if (dist > MAX_ENVCUBEMAPSCANDIST_THRESHOLD)
  {
    if (firstFree >= 0)
      nUpdate = firstFree;
    else
      nUpdate = -1;
  }
  else
  if (fDelta > fTimeInterval)
    nUpdate = firstForUse;
  if (nUpdate == -2)
  {
    // No need to update (Up to date)
    return &CTexture::m_EnvCMaps[firstForUse];
  }
  if (nUpdate >= 0)
  {
    if (!CTexture::m_EnvCMaps[nUpdate].m_pTex->m_pTexture || gRenDev->m_RP.m_PS.m_fEnvCMapUpdateTime < 0.1f)
    {
      int n = nUpdate;
      CTexture::m_EnvCMaps[n].m_TimeLastUpdated = curTime;
      CTexture::m_EnvCMaps[n].m_CamPos = Pos;
      CTexture::ScanEnvironmentCube(&CTexture::m_EnvCMaps[n], RendFlags, -1, false);
    }
    gRenDev->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return &CTexture::m_EnvCMaps[nUpdate];
  }

  dist = 0;
  firstForUse = -1;
  for (i=0; i<MAX_ENVCUBEMAPS; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvCMaps[i];
    if (dist < curTime-cur->m_TimeLastUpdated && !cur->m_bInprogress)
    {
      dist = curTime - cur->m_TimeLastUpdated;
      firstForUse = i;
    }
  }
  if (firstForUse < 0)
  {
    gRenDev->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return NULL;
  }
  int n = firstForUse;
  CTexture::m_EnvCMaps[n].m_TimeLastUpdated = curTime;
  CTexture::m_EnvCMaps[n].m_CamPos = Pos;
  CTexture::ScanEnvironmentCube(&CTexture::m_EnvCMaps[n], RendFlags, -1, false);

  gRenDev->m_RP.m_PS.m_fEnvCMapUpdateTime += iTimer->GetAsyncCurTime()-time0;
  return &CTexture::m_EnvCMaps[n];
}

Ang3 sDeltAngles(Ang3& Ang0, Ang3& Ang1)
{
  Ang3 out;
  for (int i=0; i<3; i++)
  {
    float a0 = Ang0[i];
    a0 = (float)((360.0/65536) * ((int)(a0*(65536/360.0)) & 65535)); // angmod
    float a1 = Ang1[i];
    a1 = (float)((360.0/65536) * ((int)(a0*(65536/360.0)) & 65535));
    out[i] = a0 - a1;
  }
  return out;
}

SEnvTexture *CTexture::FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader *pSH, SRenderShaderResources *pRes, CRenderObject *pObj, bool bReflect, CRendElement *pRE, bool *bMustUpdate)
{
  SEnvTexture *cm = NULL;
  float time0 = iTimer->GetAsyncCurTime();

  int i;
  float distO = 999999;
  float adist = 999999;
  int firstForUse = -1;
  int firstFree = -1;
  Vec3 objPos;
  if (bMustUpdate)
    *bMustUpdate = false;
  if (!pObj)
    bReflect = false;
  else
  {
    if (bReflect)
    {
      Plane pl;
      pRE->mfGetPlane(pl);
      objPos = pl.MirrorPosition(Vec3(0,0,0));
    }
    else
    if (pRE)
      pRE->mfCenter(objPos, pObj);
    else
      objPos = pObj->GetTranslation();
  }
  float dist = 999999;
  for (i=0; i<MAX_ENVTEXTURES; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvTexts[i];
    if (cur->m_bReflected != bReflect)
      continue;
    float s = (cur->m_CamPos - Pos).GetLengthSquared();
    Ang3 angDelta = sDeltAngles(Angs, cur->m_Angle);
    float a = angDelta.x*angDelta.x + angDelta.y*angDelta.y + angDelta.z*angDelta.z;
    float so = 0;
    if (bReflect)
      so = (cur->m_ObjPos - objPos).GetLengthSquared();
    if (s <= dist && a <= adist && so <= distO)
    {
      dist = s;
      adist = a;
      distO = so;
      firstForUse = i;
      if (!so && !s && !a)
        break;
    }
    if (!cur->m_pTex->m_pTexture && firstFree < 0)
      firstFree = i;
  }
  if (bMustExist && firstForUse >= 0)
    return &CTexture::m_EnvTexts[firstForUse];
  if (bReflect)
    dist = distO;

  float curTime = iTimer->GetCurrTime();
  int nUpdate = -2;
  float fTimeInterval = dist * CRenderer::CV_r_envtexupdateinterval + CRenderer::CV_r_envtexupdateinterval*0.5f;
  float fDelta = curTime - CTexture::m_EnvTexts[firstForUse].m_TimeLastUpdated;
  if (bMustExist)
    nUpdate = -2;
  else
  if (dist > MAX_ENVTEXSCANDIST)
  {
    if (firstFree >= 0)
      nUpdate = firstFree;
    else
      nUpdate = -1;
  }
  else
  if (fDelta > fTimeInterval)
    nUpdate = firstForUse;
  if (nUpdate == -2)
  {
    // No need to update (Up to date)
    return &CTexture::m_EnvTexts[firstForUse];
  }
  if (nUpdate >= 0)
  {
    if (!CTexture::m_EnvTexts[nUpdate].m_pTex->m_pTexture || gRenDev->m_RP.m_PS.m_fEnvTextUpdateTime < 0.1f)
    {
      int n = nUpdate;
      CTexture::m_EnvTexts[n].m_TimeLastUpdated = curTime;
      CTexture::m_EnvTexts[n].m_CamPos = Pos;
      CTexture::m_EnvTexts[n].m_Angle = Angs;
      CTexture::m_EnvTexts[n].m_ObjPos = objPos;
      CTexture::m_EnvTexts[n].m_bReflected = bReflect;
      if (bMustUpdate)
        *bMustUpdate = true;
    }
    gRenDev->m_RP.m_PS.m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime()-time0;
    return &CTexture::m_EnvTexts[nUpdate];
  }

  dist = 0;
  firstForUse = -1;
  for (i=0; i<MAX_ENVTEXTURES; i++)
  {
    SEnvTexture *cur = &CTexture::m_EnvTexts[i];
    if (dist < curTime-cur->m_TimeLastUpdated && !cur->m_bInprogress)
    {
      dist = curTime - cur->m_TimeLastUpdated;
      firstForUse = i;
    }
  }
  if (firstForUse < 0)
  {
    return NULL;
  }
  int n = firstForUse;
  CTexture::m_EnvTexts[n].m_TimeLastUpdated = curTime;
  CTexture::m_EnvTexts[n].m_CamPos = Pos;
  CTexture::m_EnvTexts[n].m_ObjPos = objPos;
  CTexture::m_EnvTexts[n].m_Angle = Angs;
  CTexture::m_EnvTexts[n].m_bReflected = bReflect;
  if (bMustUpdate)
    *bMustUpdate = true;

  gRenDev->m_RP.m_PS.m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime()-time0;
  return &CTexture::m_EnvTexts[n];

}


//===========================================================================

void CTexture::ShutDown()
{
  int i;

  if(gRenDev->GetRenderType()==eRT_Null)	// workaround to fix crash when quitting the dedicated server - because the textures are shared
    return;																											// should be fixed soon

  SAFE_RELEASE_FORCE(m_Text_White);
  SAFE_RELEASE_FORCE(m_Text_Black);
  SAFE_RELEASE_FORCE(m_Text_FlatBump);
  SAFE_RELEASE_FORCE(m_Text_PaletteDebug);
  SAFE_RELEASE_FORCE(m_Text_ScreenNoiseMap);
	SAFE_RELEASE_FORCE(m_Text_Noise2DMap);	
	SAFE_RELEASE_FORCE(m_Text_NoiseBump2DMap);	
  SAFE_RELEASE_FORCE(m_Text_RT_2D);
  SAFE_RELEASE_FORCE(m_Text_RT_CM);
  SAFE_RELEASE_FORCE(m_Text_RT_LCM);
  SAFE_RELEASE_FORCE(m_Text_NoiseVolumeMap);
  SAFE_RELEASE_FORCE(m_Text_VectorNoiseVolMap);
  SAFE_RELEASE_FORCE(m_Text_NormalizationCubeMap);
  SAFE_RELEASE_FORCE(m_Text_TerrainLM);
  SAFE_RELEASE_FORCE(m_Text_CloudsLM);
  SAFE_RELEASE_FORCE(m_Text_WaterOcean);
  SAFE_RELEASE_FORCE(m_Text_WaterVolumeDDN);
  SAFE_RELEASE_FORCE(m_Text_WaterVolumeTemp);
  SAFE_RELEASE_FORCE(CTexture::m_Text_WaterPuddles[0]);
  SAFE_RELEASE_FORCE(CTexture::m_Text_WaterPuddles[1]);
  SAFE_RELEASE_FORCE(CTexture::m_Text_WaterPuddlesDDN);
  
  for (i=0; i<8; i++)
  {
    SAFE_RELEASE_FORCE(m_Text_FromRE[i]);
  }
  for (i=0; i<8; i++)
  {
    SAFE_RELEASE_FORCE(m_Text_ShadowID[i]);
  }
	for (i=0; i<2; i++)
	{
		SAFE_RELEASE_FORCE(m_Text_FromRE_FromContainer[i]);
	}
	for (i=0; i<NUM_SCALEFORM_TEXTURES; ++i)
	{
		SAFE_RELEASE_FORCE(m_Text_FromSF[i]);
	}
	for (i=0; i<2; i++)
	{
		SAFE_RELEASE_FORCE(m_Text_FromObj[i]);
	}

	SAFE_RELEASE_FORCE(m_Text_VolObj_Density);
	SAFE_RELEASE_FORCE(m_Text_VolObj_Shadow);

  SAFE_RELEASE_FORCE(m_Text_FromLight);
  SAFE_RELEASE_FORCE(m_Text_Flare);  
  SAFE_RELEASE_FORCE(m_Text_ScreenMap);

  for (i=0; i<MAX_ENVLIGHTCUBEMAPS; i++)
  {
    m_EnvLCMaps[i].Release();
  }
  for (i=0; i<MAX_ENVCUBEMAPS; i++)
  {
    m_EnvCMaps[i].Release();
  }
  for (i=0; i<MAX_ENVTEXTURES; i++)
  {
    m_EnvTexts[i].Release();
  }
  SAFE_RELEASE(m_Text_NoTexture);
  //m_EnvTexTemp.Release();

	SAFE_RELEASE(m_Text_AOTarget);

  if (CRenderer::CV_r_releaseallresourcesonexit)
  {
    CCryName Name = CTexture::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    if (pRL)
    {
      int n = 0;
      ResourcesMapItor itor;
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); )
      {
        CTexture *pTX = (CTexture *)itor->second;
        itor++;
        if (!pTX)
          continue;
        if (CRenderer::CV_r_printmemoryleaks)
          iLog->Log("Warning: CTexture::ShutDown: Texture %s was not deleted (%d)", pTX->GetName(), pTX->GetRefCounter());
        SAFE_RELEASE_FORCE(pTX);
        n++;
      }
    }

  }
}

bool CTexture::ReloadFile(const char *szFileName, uint nFlags)
{
  char realNameBuffer[256];	
  fpConvertDOSToUnixName(realNameBuffer, szFileName);

	char gameFolderPath[256];
	strcpy(gameFolderPath, PathUtil::GetGameFolder());
	int gameFolderPathLength = strlen(gameFolderPath);
	if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] == '\\')
	{
		gameFolderPath[gameFolderPathLength - 1] = '/';
	}
	else if (gameFolderPathLength > 0 || gameFolderPath[gameFolderPathLength - 1] != '/')
	{
		gameFolderPath[gameFolderPathLength++] = '/';
		gameFolderPath[gameFolderPathLength] = 0;
	}

	char* realName = realNameBuffer;
	if (strlen(realNameBuffer) >= gameFolderPathLength && memcmp(realName, gameFolderPath, gameFolderPathLength) == 0)
		realName += gameFolderPathLength;

  bool bRes = false;
  CCryName className = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(className);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp)
        continue;

      if (!(tp->m_nFlags & FT_FROMIMAGE))
        continue;
			char srcNameBuffer[MAX_PATH + 1];
		  fpConvertDOSToUnixName(srcNameBuffer, tp->m_SrcName.c_str());
			char* srcName = srcNameBuffer;
			if (strlen(srcName) >= gameFolderPathLength && _strnicmp(srcName, gameFolderPath, gameFolderPathLength) == 0)
				srcName += gameFolderPathLength;
			//CryLogAlways("realName = %s srcName = %s gameFolderPath = %s szFileName = %s", realName, srcName, gameFolderPath, szFileName);
      if (!stricmp(realName,srcName))
        tp->Reload(~0);
    }
  }

  return bRes;
}

void CTexture::ReloadTextures()
{
  CCryName className = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(className);
  if (pRL)
  {
    ResourcesMapItor itor;
    int nID = 0;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++, nID++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp)
        continue;
      if (!(tp->m_nFlags & FT_FROMIMAGE))
        continue;
      tp->Reload(~0);
    }
  }
}

bool CTexture::SetNoTexture()
{
  if (m_Text_NoTexture)
  {
    m_pDeviceTexture = m_Text_NoTexture->m_pDeviceTexture;
#if defined (DIRECT3D10)
    m_pDeviceShaderResource = m_Text_NoTexture->m_pDeviceShaderResource;
#endif
    m_eTFDst = m_Text_NoTexture->GetDstFormat();
    m_nMips = m_Text_NoTexture->GetNumMips();
    m_nWidth = m_Text_NoTexture->GetWidth();
    m_nHeight = m_Text_NoTexture->GetHeight();
    m_nDepth = 1;
    m_nDefState = m_Text_NoTexture->m_nDefState;

    m_bNoTexture = true;
    m_nFlags |= FT_FAILED;
    m_nDeviceSize = 0;
    return true;
  }
  return false;
}

void CTexture::SetGridTexture(int nTUnit, CTexture *tp, int nTSlot)
{
  int i, j;
  if (tp->GetWidth() < 8 && tp->GetHeight() < 8)
  {
    CTexture::m_Text_NoTexture->Apply();
    return;
  }
  for (i=0; i<m_TGrids.Num(); i++)
  {
    STexGrid *tg = &m_TGrids[i];
    if (tg->m_Width == tp->GetWidth() && tg->m_Height == tp->GetHeight() && tg->m_TP->GetTextureType() == tp->GetTextureType())
      break;
  }
  if (i != m_TGrids.Num())
  {
    m_TGrids[i].m_TP->Apply(nTUnit);
    return;
  }
  STexGrid tg;
  tg.m_Width = tp->GetWidth();
  tg.m_Height = tp->GetHeight();
  int nFlags = 0;
  if (nTSlot == EFTT_BUMP)
    nFlags |= FT_TEX_NORMAL_MAP;
  CTexture *tpg = ForName("Textures/Defaults/world_texel.tga", nFlags, eTF_Unknown);
  if (tpg->GetWidth() == tp->GetWidth() && tpg->GetHeight() == tp->GetHeight() && tpg->GetTextureType() == tp->GetTextureType())
  {
    tg.m_TP = tpg;
    tg.m_TP->Apply(nTUnit);
    m_TGrids.AddElem(tg);
    return;
  }
  byte *src = tpg->GetData32();
  int wdt = tg.m_Width;
  int hgt = tg.m_Height;
  int wdt1 = tpg->GetWidth();
  int hgt1 = tpg->GetHeight();

  SAFE_RELEASE(tpg);
  if (!src)
  {
    CTexture::m_Text_NoTexture->Apply();
    return;
  }
  byte *dst = new byte[tg.m_Width*tg.m_Height*4];

  int mwdt1 = wdt1-1;
  int mhgt1 = hgt1-1;
  for (j=0; j<hgt; j++)
  {
    for (i=0; i<wdt; i++)
    {
      dst[j*wdt*4+i*4+0] = src[(i&mwdt1)*4+(j&mhgt1)*wdt1*4+0];
      dst[j*wdt*4+i*4+1] = src[(i&mwdt1)*4+(j&mhgt1)*wdt1*4+1];
      dst[j*wdt*4+i*4+2] = src[(i&mwdt1)*4+(j&mhgt1)*wdt1*4+2];
      dst[j*wdt*4+i*4+3] = src[(i&mwdt1)*4+(j&mhgt1)*wdt1*4+3];
    }
  }
  char name[128];
  sprintf(name, "TexGrid_%d_%d", wdt, hgt);
  tg.m_TP = Create2DTexture(name, wdt, hgt, 1, FT_FORCE_MIPS, dst, eTF_A8R8G8B8, eTF_A8R8G8B8);
  delete [] dst;
  delete [] src;
  tg.m_TP->Apply(nTUnit);
  m_TGrids.AddElem(tg);
}

//===============================================================================

bool WriteJPG(byte *dat, int wdt, int hgt, const char *name);
bool WriteDDS(byte *dat, int wdt, int hgt, int Size, const char *name, ETEX_Format eF, int NumMips);


byte *sDDSData;
static TArray<byte> sDDSAData;
void WriteDTXnFile (DWORD count, void *buffer, void * userData)
{
  int n = sDDSAData.Num();
  sDDSAData.Grow(count);
  cryMemcpy(&sDDSAData[n], buffer, count);
}


//===========================================================================

void CTexture::LoadDefaultTextures()
{
  char str[256];
  int i;

  m_Text_NoTexture = CTexture::ForName("Textures/Defaults/replaceme.dds", FT_DONT_RELEASE | FT_DONT_RESIZE | FT_DONT_STREAM, eTF_Unknown);
  m_Text_White = CTexture::ForName("Textures/Defaults/White.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
  m_Text_Gray = CTexture::ForName("Textures/Defaults/Grey.dds", FT_DONT_RELEASE| FT_DONT_RESIZE, eTF_Unknown);
  m_Text_Black = CTexture::ForName("Textures/Defaults/Black.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
  m_Text_FlatBump = CTexture::ForName("Textures/Defaults/White_ddn.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_TEX_NORMAL_MAP, eTF_Unknown);
  m_Text_PaletteDebug = CTexture::ForName("Textures/Defaults/palletteInst.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE, eTF_Unknown);
  m_Text_IconShaderCompiling = CTexture::ForName("Textures/Defaults/IconShaderCompile.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE | FT_DONT_STREAM, eTF_Unknown);
  m_Text_IconStreaming = CTexture::ForName("Textures/Defaults/IconStreaming.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE | FT_DONT_STREAM, eTF_Unknown);
  m_Text_IconStreamingTerrainTexture = CTexture::ForName("Textures/Defaults/IconStreamingTerrain.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE | FT_DONT_STREAM, eTF_Unknown);

//	m_Text_ScreenNoiseMap = CTexture::ForName("Textures/Defaults/dither_pattern.dds", FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown);
	m_Text_ScreenNoiseMap = CTexture::ForName("Textures/Defaults/JumpNoiseHighFrequency_x27y19.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE, eTF_Unknown);
  
  // fixme: get texture resolution from CREWaterOcean 
  m_Text_WaterOcean = CTexture::CreateTextureObject("$WaterOceanMap", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_NOMIPS | FT_USAGE_DYNAMIC|FT_DONT_STREAM, eTF_Unknown, TO_WATEROCEANMAP);
  m_Text_WaterVolumeTemp = CTexture::CreateTextureObject("$WaterVolumeTemp", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_NOMIPS | FT_USAGE_DYNAMIC|FT_DONT_STREAM, eTF_Unknown);
  m_Text_WaterVolumeDDN = CTexture::CreateTextureObject("$WaterVolumeDDN", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown,  TO_WATERVOLUMEMAP);

	m_Text_Noise2DMap = CTexture::ForName("Textures/Defaults/screen_noisy.dds", FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown);
	m_Text_NoiseBump2DMap = CTexture::ForName("Textures/Defaults/screen_noisy_bump.dds", FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown);

  // Default Template textures
  int nRTFlags = FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_STREAM | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET;
  m_Text_ScreenMap = CTexture::CreateTextureObject("$ScreenTexMap", 0, 0, 1, eTT_2D, nRTFlags, eTF_Unknown, TO_SCREENMAP);
  m_Text_MipColors_Diffuse = CTexture::CreateTextureObject("$MipColors_Diffuse", 0, 0, 1, eTT_2D, FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown, TO_MIPCOLORS_DIFFUSE);
  m_Text_MipColors_Bump = CTexture::CreateTextureObject("$MipColors_Bump", 0, 0, 1, eTT_2D, FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown, TO_MIPCOLORS_BUMP);

  m_Text_RT_2D = CTexture::CreateTextureObject("$RT_2D", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_RT_2D);
  m_Text_RT_CM = CTexture::CreateTextureObject("$RT_CM", 0, 0, 1, eTT_AutoCube, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_RT_CM);
  m_Text_RT_LCM = CTexture::CreateTextureObject("$RT_LCM", 0, 0, 1, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_RT_LCM);
  m_Text_FromLight = CTexture::CreateTextureObject("$FromLightCM", 0, 0, 1, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_FROMLIGHT);

  CTexture::m_Text_WaterPuddles[0] = CTexture::CreateTextureObject("$WaterPuddlesPrev", 256, 256, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
  CTexture::m_Text_WaterPuddles[1] = CTexture::CreateTextureObject("$WaterPuddlesNext", 256, 256, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
  CTexture::m_Text_WaterPuddlesDDN = CTexture::CreateTextureObject("$WaterPuddlesDDN", 256, 256, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_WATERPUDDLESMAP);

	for (i=0; i<2; i++)
	{
		if (i)
			sprintf(str, "$FromObj%d", i);
		else
			strcpy(str, "$FromObj");	
		m_Text_FromObj[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_FROMOBJ0+i);
	}

  m_Text_ZTarget = CTexture::CreateTextureObject("$ZTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM| FT_DONT_GENNAME, eTF_Unknown);

  m_Text_ZTargetMS = CTexture::CreateTextureObject("$ZTargetMS", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM| FT_DONT_GENNAME, eTF_Unknown, TO_ZTARGET_MS);

  m_Text_ZTargetScaled = CTexture::CreateTextureObject("$ZTargetScaled", 0, 0, 1, eTT_2D, 
      FT_DONT_RELEASE | FT_DONT_STREAM| FT_DONT_GENNAME, eTF_Unknown, TO_DOWNSCALED_ZTARGET_FOR_AO);

  m_Text_SceneTarget = CTexture::CreateTextureObject("$SceneTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_GENNAME, eTF_Unknown);
  m_Text_SceneTargetFSAA = CTexture::CreateTextureObject("$SceneTargetFSAA", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_GENNAME, eTF_Unknown);

	m_Text_RT_ShadowPool = CTexture::CreateTextureObject("$RT_ShadowPool", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

	m_Text_RT_NULL = CTexture::CreateTextureObject("$RT_NULL", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

  // Create dummy texture object for terrain and clouds lightmap
  m_Text_TerrainLM = CTexture::CreateTextureObject("$TerrainLM", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_TERRAIN_LM);
  m_Text_CloudsLM = CTexture::CreateTextureObject("$CloudsLM", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_CLOUDS_LM);

  m_Text_ScatterLayer = CTexture::CreateTextureObject("$ScatterLayer", 0, 0, 1, eTT_2D, nRTFlags, eTF_A16B16G16R16F, TO_SCATTER_LAYER, 95);

  for (i=0; i<MAX_REND_LIGHT_GROUPS; i++)
  {
    sprintf(str, "$ScreenShadowMap_%d", i);
    m_Text_ScreenShadowMap[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, nRTFlags, eTF_A8R8G8B8, TO_SCREENSHADOWMAP, 95);
    m_Tex_CurrentScreenShadowMap[i] = m_Text_ScreenShadowMap[i];
  }

	m_Text_AOTarget = CTexture::CreateTextureObject("$AOTarget", 0, 0, 1, eTT_2D, nRTFlags, eTF_A8R8G8B8, TO_SCREEN_AO_TARGET, 95);

  for (i=0; i<4; i++)
  {
    if (i)
      sprintf(str, "$LightInfo_%d", i);
    else
      strcpy(str, "$LightInfo");
    uint nFlags = FT_DONT_ANISO | FT_DONT_STREAM| FT_USAGE_DYNAMIC | FT_DONT_RELEASE;
    m_Text_LightInfo[i] = CTexture::CreateTextureObject(str, 64, 8, 1, eTT_2D, nFlags, eTF_A32B32G32R32F, TO_LIGHTINFO);
  }

  for (i=0; i<8; i++)
  {
    sprintf(str, "$FromRE_%d", i);
    m_Text_FromRE[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_FROMRE0+i);
  }

  for (i=0; i<8; i++)
  {
    sprintf(str, "$ShadowID_%d", i);
    m_Text_ShadowID[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_SHADOWID0+i);
  }

	for (i=0; i<2; i++)
	{
		sprintf(str, "$FromRE%d_FromContainer", i);
		m_Text_FromRE_FromContainer[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_FROMRE0_FROM_CONTAINER+i);
	}

	for (i=0; i<NUM_SCALEFORM_TEXTURES; ++i)
	{
		sprintf(str, "$FromSF%d", i);
		m_Text_FromSF[i] = CTexture::CreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_FROMSF0+i);
	}

	m_Text_VolObj_Density = CTexture::CreateTextureObject("$VolObj_Density", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_VOLOBJ_DENSITY);
	m_Text_VolObj_Shadow = CTexture::CreateTextureObject("$VolObj_Shadow", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_VOLOBJ_SHADOW);

	for (i=0; i<MAX_ENVLIGHTCUBEMAPS; i++)
  {
    sprintf(str, "$EnvLCMap_%d", i);
    m_EnvLCMaps[i].m_Id = i;
    m_EnvLCMaps[i].m_pTex = new SDynTexture(0, 0, eTF_A8R8G8B8, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM, str);
  }  
  for (i=0; i<MAX_ENVCUBEMAPS; i++)
  {
    sprintf(str, "$EnvCMap_%d", i);
    m_EnvCMaps[i].m_Id = i;
    m_EnvCMaps[i].m_pTex = new SDynTexture(0, 0, eTF_A8R8G8B8, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM, str);
  }  
  //m_EnvTexTemp.m_Id = 0;
  //m_EnvTexTemp.m_pTex = new SDynTexture(0, 0, eTF_A8R8G8B8, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_COMPRESS, "$EnvCMap_Temp");
  for (i=0; i<MAX_ENVTEXTURES; i++)
  {
    sprintf(str, "$EnvTex_%d", i);
    m_EnvTexts[i].m_Id = i;
    m_EnvTexts[i].m_pTex = new SDynTexture(0, 0, eTF_A8R8G8B8, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, "EnvLCMap");
  }  
  for (i=0; i<EFTT_MAX; i++)
  {
    m_Templates[i].SetCustomID(EFTT_DIFFUSE + i);
    m_Templates[i].SetFlags(FT_DONT_RELEASE);
  }


  GenerateFuncTextures();
}

//////////////////////////////////////////////////////////////////////////
const char* CTexture::GetFormatName()
{
	return NameForTextureFormat(GetDstFormat());
}

//////////////////////////////////////////////////////////////////////////
const char* CTexture::GetTypeName()
{
	return NameForTextureType(GetTextureType());
}

//////////////////////////////////////////////////////////////////////////
CDynTextureSource::CDynTextureSource(ETexPool eTexPool)
: m_refCount(1)
, m_lastUpdateTime(0)
, m_pDynTexture(0)
{
	int width(0), height(0);
	CalcSize(width, height);
	m_pDynTexture = new SDynTexture2(width, height, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO|FT_STATE_CLAMP|FT_NOMIPS, "DynTextureSource", eTexPool);
}

//////////////////////////////////////////////////////////////////////////
CDynTextureSource::~CDynTextureSource()
{
	SAFE_DELETE(m_pDynTexture);
}

//////////////////////////////////////////////////////////////////////////
void CDynTextureSource::AddRef()
{
	++m_refCount;
}

//////////////////////////////////////////////////////////////////////////
void CDynTextureSource::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
void CDynTextureSource::CalcSize(int& width, int& height, float distToCamera) const
{
	int size;
	int logSize;
	switch(CRenderer::CV_r_envtexresolution)
	{
	case 0:
	case 1:
		size = 256;
		logSize = 8;
		break;
	case 2:
	case 3:
	default:
		size = 512;
		logSize = 9;
		break;
	}

	if (distToCamera > 0)
	{
		int x(0), y(0), width(1), height(1);
		gRenDev->GetViewport(&x, &y, &width, &height);
		float lod = logf(max(distToCamera * 1024.0f / (float) max(width, height), 1.0f));
		int lodFixed = fastround_positive(lod);
		size = 1 << max(logSize - lodFixed, 5);
	}

	width = size;
	height = size;
}

//////////////////////////////////////////////////////////////////////////
bool CDynTextureSource::Apply(int nTUnit, int nTS)
{
	assert(m_pDynTexture);
	if (!m_pDynTexture || !m_pDynTexture->IsValid())
		return false;

	m_pDynTexture->Apply(nTUnit, nTS);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDynTextureSource::GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const
{
	assert(m_pDynTexture);
	if (!m_pDynTexture || !m_pDynTexture->IsValid())
	{
		offsX = 0;
		offsY = 0;
		scaleX = 1;
		scaleY = 1;
		return;
	}

	uint32 x, y, w, h;
	m_pDynTexture->GetSubImageRect(x, y, w, h);

	ITexture* pSrcTex(m_pDynTexture->GetTexture());
	float invSrcWidth(1.0f / (float) pSrcTex->GetWidth());
	float invSrcHeight(1.0f / (float) pSrcTex->GetHeight());
	offsX  = x * invSrcWidth;
	offsY  = y * invSrcHeight;
	scaleX = w * invSrcWidth;
	scaleY = h * invSrcHeight;
}

//////////////////////////////////////////////////////////////////////////
CFlashTextureSource::CFlashTextureSource(const char* pFlashFileName)
: CDynTextureSource(eTP_Clouds)
, m_pFlashPlayer(0)
#ifdef _DEBUG
, m_flashSrcFile(pFlashFileName)
#endif
{
	if (pFlashFileName)
	{
		m_pFlashPlayer = gEnv->pSystem->CreateFlashPlayerInstance();
		if (m_pFlashPlayer)
		{
			if (!m_pFlashPlayer->Load(pFlashFileName, IFlashPlayer::DEFAULT_NO_MOUSE))
			{
				SAFE_RELEASE(m_pFlashPlayer);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CFlashTextureSource::~CFlashTextureSource()
{
	SAFE_RELEASE(m_pFlashPlayer);
}

//////////////////////////////////////////////////////////////////////////
void CFlashTextureSource::GetDynTextureSource(void*& pIDynTextureSource, EDynTextureSource& dynTextureSource)
{	
	pIDynTextureSource = m_pFlashPlayer;
	dynTextureSource = DTS_I_FLASHPLAYER;
}

//////////////////////////////////////////////////////////////////////////
CVideoTextureSource::CVideoTextureSource(const char* pVideoFileName)
: CDynTextureSource(eTP_Clouds)
, m_pVideoPlayer(0)
#ifdef _DEBUG
, m_videoSrcFile(pVideoFileName)
#endif
{
	if (pVideoFileName)
	{
		m_pVideoPlayer = gRenDev->CreateVideoPlayerInstance();
		if (m_pVideoPlayer)
		{
			if (!m_pVideoPlayer->Load(pVideoFileName, IVideoPlayer::LOOP_PLAYBACK|IVideoPlayer::DELAY_START))
			{
				SAFE_RELEASE(m_pVideoPlayer);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVideoTextureSource::~CVideoTextureSource()
{
	SAFE_RELEASE(m_pVideoPlayer);
}

//////////////////////////////////////////////////////////////////////////
void CVideoTextureSource::GetDynTextureSource(void*& pIDynTextureSource, EDynTextureSource& dynTextureSource)
{	
	pIDynTextureSource = m_pVideoPlayer;
	dynTextureSource = DTS_I_VIDEOPLAYER;
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_AddRTStat(CTexture *pTex, int nFlags, int nW, int nH)
{
  SRTargetStat TS;
  int nSize;
  ETEX_Format eTF;
  if (!pTex)
  {
    eTF = eTF_A8R8G8B8;
    if (nW < 0)
      nW = m_width;
    if (nH < 0)
      nH = m_height;
    nSize = CTexture::TextureDataSize(nW, nH, 1, 1, eTF);
    TS.m_Name = "Back buffer";
  }
  else
  {
    eTF = pTex->GetDstFormat();
    if (nW < 0)
      nW = pTex->GetWidth();
    if (nH < 0)
      nH = pTex->GetHeight();
    nSize = CTexture::TextureDataSize(nW, nH, 1, pTex->GetNumMips(), eTF);
    const char *szName = pTex->GetName();
    if (szName && szName[0] == '$')
      TS.m_Name = string("@") + string(&szName[1]);
    else
      TS.m_Name = szName;
  }
  TS.m_eTF = eTF;

  if (nFlags > 0)
  {
    if (nFlags == 1)
      TS.m_Name += " (Target)";
    else
    if (nFlags == 2)
    {
      TS.m_Name += " (Depth)";
      nSize = nW * nH * 3;
    }
    else
    if (nFlags == 4)
    {
      TS.m_Name += " (Stencil)";
      nSize = nW * nH;
    }
    else
    if (nFlags == 3)
    {
      TS.m_Name += " (Target + Depth)";
      nSize += nW * nH * 3;
    }
    else
    if (nFlags == 6)
    {
      TS.m_Name += " (Depth + Stencil)";
      nSize = nW*nH*4;
    }
    else
    if (nFlags == 5)
    {
      TS.m_Name += " (Target + Stencil)";
      nSize += nW*nH;
    }
    else
    if (nFlags == 7)
    {
      TS.m_Name += " (Target + Depth + Stencil)";
      nSize += nW*nH*4;
    }
    else
    {
      assert(0);
    }
  }
  TS.m_nSize = nSize;
  TS.m_nWidth = nW;
  TS.m_nHeight = nH;

  m_RP.m_RTStats.push_back(TS);
}

void CRenderer::EF_PrintRTStats(const char *szName)
{
  const int nYstep = 14;
  int nY = 30; // initial Y pos
  int nX = 20; // initial X pos
  ColorF col = Col_Green;
  Draw2dLabel(nX, nY, 1.6f, &col.r, false, szName);
  nX += 10; nY += 25;

  col = Col_White;
  int nYstart = nY;
  int nSize = 0;
  for (int i=0; i<m_RP.m_RTStats.size(); i++)
  {
    SRTargetStat *pRT = &m_RP.m_RTStats[i];

    Draw2dLabel(nX, nY, 1.4f, &col.r, false, "%s (%d x %d x %s), Size: %.3f Mb", pRT->m_Name.c_str(), pRT->m_nWidth, pRT->m_nHeight, CTexture::NameForTextureFormat(pRT->m_eTF), (float)pRT->m_nSize / 1024.0f / 1024.0f);
    nY += nYstep;
    if (nY >= m_height)
    {
      nY = nYstart;
      nX += 300;
    }
    nSize += pRT->m_nSize;
  }
  col = Col_Yellow;
  Draw2dLabel(nX, nY+10, 1.4f, &col.r, false, "Total: %d RT's, Size: %.3f Mb", m_RP.m_RTStats.size(), nSize / 1024.0f / 1024.0f);
}

bool CTexture::IsFSAAChanged()
{
#if defined(DIRECT3D9) || defined(DIRECT3D10) || defined(OPENGL)
  if (m_nFSAASamples != gRenDev->m_RP.m_FSAAData.Type)
    return true;
  if (m_nFSAAQuality != gRenDev->m_RP.m_FSAAData.Quality)
    return true;
#endif
  return false;
}
