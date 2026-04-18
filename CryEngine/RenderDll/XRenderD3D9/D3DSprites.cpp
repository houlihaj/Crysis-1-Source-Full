#include "StdAfx.h"
#include "DriverD3D.h"

//=========================================================================================

#include <IStatObj.h>
#include <I3DEngine.h>
#include <IEntityRenderState.h>
#include "../Cry3DEngine/Cry3DEngineBase.h"
#include "../Cry3DEngine/Array2d.h"
#include "../Cry3DEngine/terrain.h"
#include "../Cry3DEngine/StatObj.h"
#include "../Cry3DEngine/ObjMan.h"
#include "../Cry3DEngine/Vegetation.h"
#include "../Cry3DEngine/terrain_sector.h"
#include "../Cry3DEngine/ObjectsTree.h"

#ifdef XENON
#include "xgraphics.h"
#endif

bool WriteTGA(byte *dat, int wdt, int hgt, char *name);

//SDynTexture2 *gDT;
//int gnFrame;


static TArray<SSpriteInfo> sSPInfo;
extern CTexture *gTexture;

void sSetViewPort(int nCurX, int nCurY, int nSpriteRes)
{
	// one pixel border for proper border handling
  gcpRendD3D->SetViewport(nCurX*nSpriteRes+1, nCurY*nSpriteRes+1, nSpriteRes-3, nSpriteRes-3);
}

static const float sf60Sin = sin(60.0f*(gf_PI/180.0f));
static const float sf60Cos = cos(60.0f*(gf_PI/180.0f));


void sFlushSprites(SSpriteGenInfo *pSGI, int nCurX, int nCurY, SDynTexture *pSrcTex, int nTexSizeInt, int nTexSizeFinal)
{
  int n = 0;
  int ncX = 0;
  int ncY = 0;
  CD3D9Renderer *r = gcpRendD3D;
  STexState pTexState;
  pTexState.SetFilterMode(FILTER_POINT);        
  pTexState.SetClampMode(TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP);
  int nTS = CTexture::GetTexState(pTexState);
  r->Set2DMode(true, 1, 1);
  CTexture *pCurRT = r->m_pNewTarget[0]->m_pTex;

  SDynTexture2 *pNewTex;
  while (true)
  {
    SSpriteGenInfo *pSG = &pSGI[n++];
    SSpriteInfo *pSP = &sSPInfo[pSG->nSP];
    /*if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
    {
      int nnn = 0;
    }*/

    // dilate rendered sprite and put into sprite texture -------------------
    pNewTex = (SDynTexture2 *)*pSG->ppTexture;
    assert (pNewTex);
    if (!pNewTex)
    {
      r->Set2DMode(false, 1, 1);
      return;
    }

		pNewTex->Update(nTexSizeFinal, nTexSizeFinal);

    if (!pNewTex->m_pTexture || !pNewTex->m_pTexture->GetDeviceTexture())
    {
      assert(0);
      SAFE_RELEASE(pNewTex);
      *pSG->ppTexture = NULL;
      return;
    }
    *pSG->ppTexture = pNewTex;

    uint32 nX, nY, nW, nH;
    pNewTex->GetSubImageRect(nX, nY, nW, nH);

    uint32 nX1, nY1, nW1, nH1;
    pNewTex->GetImageRect(nX1, nY1, nW1, nH1);

    CTexture *pT = (CTexture *)pNewTex->GetTexture();
    pT->m_nAccessFrameID = r->m_nFrameUpdateID;
    pSP->m_pTex = pNewTex;
    const uint32 dwWidth = pT->GetWidth();
    const uint32 dwHeight = pT->GetHeight();

    pSP->m_ucTexCoordMinX = (nX*128)/dwWidth;					assert((pSP->m_ucTexCoordMinX*dwWidth)/128==nX);
    pSP->m_ucTexCoordMinY = (nY*128)/dwHeight;				assert((pSP->m_ucTexCoordMinY*dwHeight)/128==nY);
    pSP->m_ucTexCoordMaxX = ((nX+nW)*128)/dwWidth;		assert((pSP->m_ucTexCoordMaxX*dwWidth)/128==nX+nW);
    pSP->m_ucTexCoordMaxY = ((nY+nH)*128)/dwHeight;		assert((pSP->m_ucTexCoordMaxY*dwHeight)/128==nY+nH);

    CShader *pPostEffects = CShaderMan::m_shPostEffects;

		gcpRendD3D->EF_ApplyShaderQuality(eST_PostProcess);		// needed for GetShaderQuality()

    r->LogStrv(SRendItem::m_RecurseLevel, "Generating '%s' - %s (%d, %d, %d, %d) (%d)\n", pT->GetName(), pNewTex->IsSecondFrame() ? "Second" : "First", nX, nY, nW, nH, r->GetFrameID(false));

    if (pCurRT != pT)
    {
      pCurRT = pT;
      pNewTex->SetRT(0, false, r->FX_GetDepthSurface(nW1, nH1, false));
    }
    else
      pNewTex->SetRectStates();
    bool bOk = pPostEffects->FXSetTechnique("Dilate");
    r->EF_Commit();

    assert(bOk);

    uint nPasses = 0;
    pPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pPostEffects->FXBeginPass(0);

    r->EF_SetState(GS_NODEPTHTEST);

    float fInvBrightnessMultiplier = 1.0f/pSG->fBrightnessMultiplier;

    Vec4 v;
    v[1] = v[2] = v[3] = 0;
    v[0] = fInvBrightnessMultiplier;
		static CCryName vDilateParamsName("vDilateParams");
    pPostEffects->FXSetPSFloat(vDilateParamsName, &v, 1); 

    float fTexSize = (float)nTexSizeInt;
    float fHalfX = 1.0f / (float)pSrcTex->GetWidth();
    float fHalfY = 1.0f / (float)pSrcTex->GetHeight();
    float fSizeX = fTexSize * fHalfX;
    float fSizeY = fTexSize * fHalfY;
    float fOffsX = (float)ncX*fSizeX;
    float fOffsY = (float)ncY*fSizeY;
#if defined(DIRECT3D9) || defined(OPENGL)
    fOffsX += fHalfX*0.5f;
    fOffsY += fHalfY*0.5f;
#endif
    //fHalfX *= 0.5f;
    //fHalfY *= 0.5f;

    v[0] = fHalfX;
    v[1] = fHalfY;
    v[2] = fSizeX;
		static CCryName vPixelOffsetName("vPixelOffset");
    pPostEffects->FXSetPSFloat(vPixelOffsetName, &v, 1);

    pSrcTex->Apply(0, nTS);

    float fZ = 0.5f;
    r->DrawQuad3D(Vec3(0,0,fZ), Vec3(1,0,fZ), Vec3(1,1,fZ), Vec3(0,1,fZ), Col_White, fOffsX, fOffsY, fOffsX+fSizeX, fOffsY+fSizeY);

    pPostEffects->FXEndPass();

//    pNewTex->RestoreRT(0, false);

    ncX += 2;
    if ((ncX+2)*nTexSizeInt > pSrcTex->GetWidth())
    {
      ncX = 0;
      ncY++;
    }
    if (ncX==nCurX && ncY==nCurY)
      break;
  }

  r->Set2DMode(false, 1, 1);
  gcpRendD3D->EF_Scissor(false, 0, 0, 0, 0);
}

void CD3D9Renderer::MakeSprites(TArray<SSpriteGenInfo>& SGI)
{
	PROFILE_FRAME(MakeSprites);

#ifdef XENON
  return;
#endif

  SDynTexture *pTempTex = NULL;
  bool bHDR = IsHDRModeEnabled();
  ETEX_Format eTF = bHDR ? eTF_A16B16G16R16F : eTF_A8R8G8B8;
  int nWidth = GetWidth();
  int nHeight = GetHeight();
  pTempTex = new SDynTexture(nWidth, nHeight, eTF, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempSpr", 95);
  pTempTex->Update(nWidth, nHeight);
  if (!pTempTex || !pTempTex->m_pTexture || !pTempTex->m_pTexture->GetDeviceTexture())
    return;
  int nSpriteResFinal = CV_r_VegetationSpritesTexRes;  
  int nSpriteResInt = nSpriteResFinal << 1;
  int nSprX = pTempTex->GetWidth() / nSpriteResInt;
  int nSprY = pTempTex->GetHeight() / nSpriteResInt;
  int nCurX = 0;
  int nCurY = 0;
  SD3DSurface *pDepthSurfFSAA = &m_DepthBufferOrigFSAA;
  SD3DSurface *pDepthSurf = &m_DepthBufferOrig;
  if (m_RP.m_FSAAData.Type > 1)
  {
    assert(pTempTex->m_pTexture->GetFlags() & FT_USAGE_RENDERTARGET);
  }
  if (SRendItem::m_RecurseLevel > 1)
    pDepthSurf = FX_GetDepthSurface(pTempTex->GetWidth(), pTempTex->GetHeight(), false);

  int i;
  SRendParams rParms;
  CShader *sh = m_RP.m_pShader;
  SRenderShaderResources *pRes = m_RP.m_pShaderResources;
  CRenderObject *pObj = m_RP.m_pCurObject;
  SShaderTechnique *pTech = m_RP.m_pRootTechnique;
  CRendElement *pRE = m_RP.m_pRE;

  int nPrevStr = CV_r_texturesstreamingsync;
  int nPrevAsync = CV_r_shadersasynccompiling;
  CV_r_texturesstreamingsync = 1;
  CV_r_shadersasynccompiling = 0;
  int nPrevZpass = CV_r_usezpass;
  CV_r_usezpass = 0;

  rParms.dwFObjFlags |= FOB_TRANS_MASK;
  rParms.nDLightMask = 1;
  rParms.fRenderQuality = 0.0f;
  rParms.pRenderNode = (struct IRenderNode*)(uint32)-1; // avoid random skipping of rendering

  Vec3 cSkyBackup = gEnv->p3DEngine->GetSkyColor();
  Vec3 cSunBackup = gEnv->p3DEngine->GetSunColor();
  ColorF white(1,1,1,0);

  CDLight light;
  light.SetLightColor(cSunBackup);
  light.m_SpecMult = 0;
  light.m_fRadius  = 100000000;
  light.m_Flags |= DLF_DIRECTIONAL | DLF_SUN | DLF_THIS_AREA_ONLY | DLF_LM | DLF_SPECULAROCCLUSION | DLF_CASTSHADOW_MAPS;

  int vX, vY, vWidth, vHeight;
  GetViewport(&vX, &vY, &vWidth, &vHeight);

  CCamera origCam = GetCamera();
  uint saveFlags = m_RP.m_PersFlags;
  uint saveFlags2 = m_RP.m_PersFlags2;
  m_RP.m_PersFlags |= RBPF_MAKESPRITE;
  m_RP.m_PersFlags2 &= ~(RBPF2_NOALPHABLEND| RBPF2_NOALPHATEST);

  EF_PushFog();
  EF_PushMatrix();
  m_matProj->Push();
  EnableFog(false);

  const Vec3 vEye(0,0,0);
  const Vec3 vAt(-1,0,0);
  const Vec3 vUp(0,0,1);
  Matrix44 mView;  
  mathMatrixLookAt(&mView, vEye, vAt, vUp);
  pTempTex->SetRT(0, true, pDepthSurf);
  SetViewport(0, 0, m_width, m_height);
  EF_ClearBuffers(FRT_CLEAR, &white);
  EF_Commit();

  int nStart = 0;
  m_RP.m_PS.m_NumSpriteUpdates += SGI.Num();
  for (i=0; i<SGI.Num(); i++)
  {
    SSpriteGenInfo &SG = SGI[i];
    SSpriteInfo *pSP = &sSPInfo[SG.nSP];
    /*if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
    {
      int nnn = 0;
    }*/
    if (CV_r_VegetationSpritesNoGen && CTexture::m_Text_NoTexture)
    {
      SAFE_DELETE(*SG.ppTexture);
      SSpriteInfo *pSP = &sSPInfo[SG.nSP];
      pSP->m_pTex = NULL;
      continue;
    }
    if (!nCurX && !nCurY && i)
    {
      pTempTex->SetRT(0, false, pDepthSurf);
      SetViewport(0, 0, m_width, m_height);
      EF_ClearBuffers(FRT_CLEAR, &white);
      EF_Commit();
    }

    rParms.nMaterialLayers = SG.nMaterialLayers;  // pass material layers info
    rParms.pMaterial = SG.pMaterial;

    assert(SG.fBrightnessMultiplier>0);

	  if (CV_r_log)
		  Logv(SRendItem::m_RecurseLevel, "****************** CD3D9Renderer::MakeSprite - Begin ******************\n");

	  float fRadiusHors = SG.pStatObj->GetRadiusHors();
	  float fRadiusVert = SG.pStatObj->GetRadiusVert();

		// adjust vertical extend with vertical tilt
		{
			assert(SG.fAngle2==0 || SG.fAngle2==60.0f);

			bool bAngle2Below35 = SG.fAngle2<30.0f;

      float fS = bAngle2Below35 ? 0 : sf60Sin;
      float fC = bAngle2Below35 ? 1 : sf60Cos;

			fRadiusVert = fC*fRadiusVert+fS*fRadiusHors;	// seen from an angle the height becomes bigger
		}

	  Vec3 vCenter = (SG.pStatObj->GetBoxMax()+SG.pStatObj->GetBoxMin())*0.5f;

	  float fFOV = 0.565f/SG.fGenDist*200.f*(gf_PI/180.0f);
	  float fDrawDist = fRadiusVert*SG.fGenDist;

	  // make fake camera to pass some camera into to the rendering pipeline
	  CCamera tmpCam = origCam;
	  Matrix33 matRot = Matrix33::CreateOrientation(vAt-vEye, vUp, 0);
	  tmpCam.SetMatrix(Matrix34(matRot, Vec3(0,0,0)));
	  tmpCam.SetFrustum(nSpriteResInt, nSpriteResInt, fFOV, max(0.1f, fDrawDist-fRadiusHors), fDrawDist+fRadiusHors);
	  SetCamera(tmpCam);

    Matrix44 *m = m_matProj->GetTop();
    mathMatrixPerspectiveFov(m, fFOV, fRadiusHors/fRadiusVert, fDrawDist-fRadiusHors, fDrawDist+fRadiusHors);
    m_RP.m_PersFlags |= RBPF_FP_MATRIXDIRTY;

    *m_matView->GetTop() = mView;
    m_CameraZeroMatrix = mView;

    EF_SetCameraInfo();
  
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  // Start sprite gen
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  	float globalMultiplier( 1 );

	  // setup environment ------------------
	  Matrix34 matTrans;
	  matTrans.SetIdentity();
	  matTrans.SetTranslation(Vec3(-fDrawDist,0,0));

	  Matrix34 matRotation;
	  matRotation = matRotation.CreateRotationZ((SG.fAngle)*(gf_PI/180.0f));

	  Matrix34 matRotation2;
	  matRotation2 = matRotation2.CreateRotationY((SG.fAngle2)*(gf_PI/180.0f));

	  matRotation = matRotation2*matRotation;

	  Matrix34 matCenter; matCenter.SetIdentity(); matCenter.SetTranslation(-vCenter);
	  matCenter = matRotation * matCenter;

	  Matrix34 mat = matTrans*matCenter;

	  rParms.pMatrix = &mat;

	  // sun (rotate sun direction)
	  light.m_Origin = matRotation.TransformVector(gEnv->p3DEngine->GetSunDir());

    {
      rParms.AmbientColor = Col_Black;
      gEnv->p3DEngine->SetSkyColor(Vec3(0,0,0));

      EF_StartEf();  
      sSetViewPort(nCurX, nCurY, nSpriteResInt);
      EF_ADDDlight( &light );
      SG.pStatObj->Render(rParms);
      EF_EndEf3D(0);

      gEnv->p3DEngine->SetSkyColor(cSkyBackup);

      nCurX++;
    }
	  {
		  gEnv->p3DEngine->SetSunColor(Vec3(0,0,0));
      rParms.AmbientColor = gEnv->p3DEngine->GetSkyColor();
      rParms.AmbientColor.a = 1.f;

		  EF_StartEf();  
      sSetViewPort(nCurX, nCurY, nSpriteResInt);
		  SG.pStatObj->Render(rParms);
		  EF_EndEf3D(0);

		  gEnv->p3DEngine->SetSunColor(cSunBackup);

      nCurX++;
	  }

    if ((nCurX+2)*nSpriteResInt > pTempTex->GetWidth())
    {
      nCurX = 0;
      nCurY++;
      if ((nCurY+1)*nSpriteResInt > pTempTex->GetHeight())
      {
        sFlushSprites(&SGI[nStart], nCurX, nCurY, pTempTex, nSpriteResInt, nSpriteResFinal);
        nStart = i+1;
        nCurY = 0;
      }
    }
  }
  if (nCurX || nCurY)
    sFlushSprites(&SGI[nStart], nCurX, nCurY, pTempTex, nSpriteResInt, nSpriteResFinal);
  
  pTempTex->RestoreRT(0, true);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// End sprite gen
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  SAFE_DELETE(pTempTex);

	EF_PopMatrix();
	m_matProj->Pop();

  gEnv->p3DEngine->SetSkyColor(cSkyBackup);
  gEnv->p3DEngine->SetSunColor(cSunBackup);
  CV_r_usezpass = nPrevZpass;

  CV_r_texturesstreamingsync = nPrevStr;
  CV_r_shadersasynccompiling = nPrevAsync;

  SetViewport(vX, vY, vWidth, vHeight);
  EF_PopFog();
  EF_Commit();

  //char name[256];
  //sprintf(name, "%s.dds", pTempTex->GetName());
  //pTempTex->SaveDDS(name, false);

  //IDynTexture *pD = (*SGI[0].ppTexture);
  //CTexture *pDst = (CTexture *)pD->GetTexture();
  //sprintf(name, "%s.dds", pDst->GetName());
  //pDst->SaveDDS(name, false); 

  m_RP.m_PersFlags = saveFlags;
	m_RP.m_PersFlags2 = saveFlags2;

	SetCamera(origCam);

	if (CV_r_log)
		Logv(SRendItem::m_RecurseLevel, "****************** CD3D9Renderer::MakeSprite - End ******************\n");

	m_RP.m_PersFlags &= ~RBPF_MAKESPRITE;
  CHWShader_D3D::mfSetGlobalParams();

  if (SRendItem::m_RecurseLevel == 1)
    EF_ClearBuffers(FRT_CLEAR, NULL);

  m_RP.m_pShader = sh;
  m_RP.m_pShaderResources = pRes;
  m_RP.m_pCurObject = pObj;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pRootTechnique = pTech;
  m_RP.m_pRE = pRE;
}



#ifndef _LIB

// duplicated definition (first one is in 3dengine)
ISystem * Cry3DEngineBase::m_pSystem=0;
IRenderer * Cry3DEngineBase::m_pRenderer=0;
ITimer * Cry3DEngineBase::m_pTimer=0;
ILog * Cry3DEngineBase::m_pLog=0;
IPhysicalWorld * Cry3DEngineBase::m_pPhysicalWorld=0;
IConsole * Cry3DEngineBase::m_pConsole=0;
C3DEngine * Cry3DEngineBase::m_p3DEngine=0;
CVars * Cry3DEngineBase::m_pCVars=0;
ICryPak * Cry3DEngineBase::m_pCryPak=0;

#endif // _LIB

_inline bool CompareSpriteItem(const SSpriteInfo& p1, const SSpriteInfo& p2)
{
	if(p1.m_pTerrainTexInfo != p2.m_pTerrainTexInfo)
		return p1.m_pTerrainTexInfo < p2.m_pTerrainTexInfo;

  return p1.m_pTex < p2.m_pTex;
}


void CD3D9Renderer::GenerateSpritesList (PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)
{
  assert(fZoomFactor>0);

  int i;

  static TArray<SSpriteGenInfo> sSGI;

  float fInvZoomFactor= 1.0f/fZoomFactor;
  I3DEngine *pEngine = gEnv->p3DEngine;

  //	const float fSunSkyRel = pEngine->GetSunRel();
  //	const float fSkySunRel = 1.f - fSunSkyRel;

  const uint32 dwSunSkyRel = (uint32)(256.0f*pEngine->GetSunRel());			assert(dwSunSkyRel<=256); // 0..256
  const uint32 dwSkySunRel = 256-dwSunSkyRel;														assert(dwSkySunRel<=256); // 0..256

  // Sorting far objects front to back since we use alphablending
  //sObjMan = pObjMan;
  //::Sort(&(*pList)[0], pList->Count());
  //pList->SortByDistanceMember_(true);

  float max_view_dist = fMaxViewDist*0.8f;
  const Vec3& vCamPos = m_rcam.Orig;
  const float rad2deg = 180.0f/PI;    

  int nR = SRendItem::m_RecurseLevel;

	int nPolygonMode = CV_r_PolygonMode;
  int nZPass = CV_r_usezpass;
  int nMO = CV_r_measureoverdraw;

	CV_r_PolygonMode = 1;
  CV_r_measureoverdraw = 0;
  CV_r_usezpass = 0;

  sSPInfo.SetUse(0);

  static uint32 s_dwOldMakeSpriteCalls=0;

  Vec3 vSunDir = pEngine->GetSunDirNormalized();
  Vec3 vSunColor = pEngine->GetSunColor();
  Vec3 vSkyColor = pEngine->GetSkyColor();

	static ICVar *pVarDebugMask = iConsole->GetCVar( "e_debug_mask" );
  int e_debug_mask = pVarDebugMask->GetIVal(); 

  static ICVar *pVarLMGen = iConsole->GetCVar("e_terrain_lm_gen_threshold");
  static ICVar *pVarDR = iConsole->GetCVar("e_vegetation_sprites_distance_ratio");

  float fCustomDistRatio = pVarDR->GetFVal();
  float fTerrainLMThreshold = pVarLMGen->GetFVal();
  float fDirThreshold = fTerrainLMThreshold*pEngine->GetSunDir().GetLength();
  float fColorThreshold = fTerrainLMThreshold;

  int nHalfAngle = (256/FAR_TEX_COUNT)/2;
  int nHalfAngle_60 = (256/FAR_TEX_COUNT_60)/2;

  int nTexSizeFinal = CV_r_VegetationSpritesTexRes;  

  sSGI.SetUse(0);
  m_RP.m_PS.m_NumSprites += pList->Count();
  for (i=pList->Count()-1; i>=0; i--)
  { 
    SVegetationSpriteInfo & vi = pList->GetAt(i);
    CVegetation * o = vi.pVegetation;

    // Prefetch next object in cache
    /*if (i)
    {
      oNext = pList->GetAt(i-1);
      cryPrefetchT0SSE(oNext);
      cryPrefetchT0SSE((byte *)oNext+16);
    }*/

//    assert(SRendItem::m_RecurseLevel>=1 && SRendItem::m_RecurseLevel-1<=2);
//    float fDistance = (((IRenderNode*)o))->m_arrfDistance[SRendItem::m_RecurseLevel-1];

    float fMaxDist = o->m_fWSMaxViewDist;// GetMaxViewDist();

    // note: move into sort by size
    if(fMaxDist > max_view_dist)
      fMaxDist = max_view_dist;

    StatInstGroup &vegetGroup = pObjMan->m_lstStaticTypes[o->m_nObjectTypeID];

    float fAmbientValue;
		if(vegetGroup.bUseTerrainColor)
			fAmbientValue = 1.f;
		else
			fAmbientValue = vegetGroup.fBrightness;
    if(fAmbientValue > 1.f)
      fAmbientValue = 1.f;

    // get full lod to get tex id and lowest lod to take size
    CStatObj * pStatObjLOD0 = vegetGroup.GetStatObj();
    CStatObj * pStatObjLODLowest = pStatObjLOD0;
//    if(pStatObjLOD0->m_nLoadedLodsNum && pStatObjLOD0->m_arrpLowLODs[pStatObjLOD0->m_nLoadedLodsNum-1] != 0)
  //    pStatObjLODLowest = pStatObjLOD0->m_arrpLowLODs[pStatObjLOD0->m_nLoadedLodsNum-1];

    assert(pStatObjLODLowest);
    if(!pStatObjLODLowest)
      continue;

    const float fScale = o->GetScale();

    Vec3 vCenter = pStatObjLODLowest->GetCenter() * fScale;

    SSpriteInfo *pSP = sSPInfo.AddIndex(1);
    pSP->m_pTex = NULL;

    float DX = o->m_vPos.x - vCamPos.x;
    float DY = o->m_vPos.y - vCamPos.y;
    float DZ = o->m_vPos.z - vCamPos.z;
    float DZ1 = vCenter.z + DZ;

    float fSpriteScaleV = fScale*pStatObjLODLowest->GetRadiusVert();
    float fSpriteScaleH = fScale*pStatObjLODLowest->GetRadiusHors();
    pSP->m_vPos = o->m_vPos + vCenter;
    /*if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
    {
      int nnn = 0;
    }*/
    //    pSP->m_nLightMask = pList->GetAt(i).usLightMask;



    float fInvDistance2d = isqrt_fast_tpl(DX*DX+DY*DY); // 1.0f/(cry_sqrtf(DX*DX+DY*DY)+0.01f);		// +0.01f to avoid division by 0

    //		float fAngle2 = -rad2deg*cry_atan2f(DZ1,cry_sqrtf(DX*DX+DY*DY));
    //		while(fAngle2<0)
    //			fAngle2=0;
    //		bool bAngle2Below35=fAngle2<35;

    bool bAngle2Below35;
    if(fInvDistance2d<=0)
      bAngle2Below35 = DZ1>0;
    else
      bAngle2Below35 = DZ1*fInvDistance2d > -0.573577;	// -sinf(35/180.0f*3.14159265f)

		// adjust vertical extend with vertical tilt
		{
			float fS = bAngle2Below35 ? 0 : sf60Sin;
			float fC = bAngle2Below35 ? 1 : sf60Cos;

			fSpriteScaleV = fC*fSpriteScaleV+fS*fSpriteScaleH;	// seen from an angle the height becomes bigger
		}

    {
      float fMul = fSpriteScaleH*fInvDistance2d;

      pSP->m_fDY = DX*fMul;
      pSP->m_fDX = DY*fMul;
    }

		// pass terrain texture info
		pSP->m_pTerrainTexInfo = vi.pTerrainTexInfoForSprite;

    int nSlotId;

    StatInstGroup::SSpriteLightInfo *pLightInfo=0;

    IDynTexture **ppTex = 0;

    if(bAngle2Below35)
    {
      nSlotId = (((vi.dwAngle+nHalfAngle)*FAR_TEX_COUNT)/256)%FAR_TEX_COUNT;

      assert(nSlotId>=0 && nSlotId<FAR_TEX_COUNT);
      pLightInfo = &vegetGroup.m_arrSSpriteLightInfo[nSlotId];
      pSP->m_AngleY = 0;
      ppTex = &vegetGroup.m_arrSpriteTexPtr[nSlotId];
    }
    else
    {
      nSlotId = (((vi.dwAngle+nHalfAngle_60)*FAR_TEX_COUNT_60)/256)%FAR_TEX_COUNT_60;

      assert(nSlotId>=0 && nSlotId<FAR_TEX_COUNT_60);
      pLightInfo = &vegetGroup.m_arrSSpriteLightInfo_60[nSlotId];
      pSP->m_AngleY = 60;
      ppTex = &vegetGroup.m_arrSpriteTexPtr_60[nSlotId];

    }
    bool bIsEqual = pLightInfo->IsEqual(pEngine->GetSunDir(), pEngine->GetSunColor(), pEngine->GetSkyColor(), fDirThreshold, fColorThreshold);
    if (CV_r_VegetationSpritesGenAlways)
      bIsEqual = false;
    if (!bIsEqual || !(*ppTex) || !(*ppTex)->IsValid() || ((*ppTex)->GetWidth() != nTexSizeFinal))
    {
      /*if (gDT)
      {
        int nnn = 0;
      }
      if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
      {
        int nnn = 0;
      }*/
      if (!*ppTex)
      {
        char *szName;
#ifdef _DEBUG
        char Name[128];
        sprintf(Name, "$SpriteTex_(%1.f %1.f %1.f)_%d_%d", pSP->m_vPos.x, pSP->m_vPos.y, pSP->m_vPos.z, bAngle2Below35, nSlotId);
        szName = Name;
#else
        szName = "$SpriteTex";
#endif
        *ppTex = new SDynTexture2(nTexSizeFinal, nTexSizeFinal, eTF_A8R8G8B8, eTT_2D, FT_DONTSYNCMULTIGPU | FT_DONT_ANISO | FT_STATE_CLAMP | FT_NOMIPS, szName, eTP_Sprites);
        (*ppTex)->Update(nTexSizeFinal, nTexSizeFinal);
      }
      else
      {
        int nnn = 0;
      }
      if (!bIsEqual)
      {
        if (*ppTex)
          (*ppTex)->ResetUpdateMask();
        pLightInfo->SetLightingData(pEngine->GetSunDir(), pEngine->GetSunColor(), pEngine->GetSkyColor());
      }
      float fGenDist = 18.f*max(0.5f, vegetGroup.fSpriteDistRatio*fCustomDistRatio);
      if (*ppTex)
        (*ppTex)->SetUpdateMask();
      SSpriteGenInfo SGI;
      SGI.fAngle = nSlotId*(pSP->m_AngleY ? FAR_TEX_ANGLE_60 : FAR_TEX_ANGLE)+90.f;
      SGI.fAngle2 = pSP->m_AngleY;
      SGI.fGenDist = fGenDist;
      SGI.fBrightnessMultiplier = pLightInfo->GetSpriteBrightnessMultiplier();
      SGI.nMaterialLayers = vegetGroup.nMaterialLayers;
      SGI.ppTexture = ppTex;
      SGI.pMaterial = vegetGroup.pMaterial;
      SGI.pStatObj = pStatObjLODLowest;
      SGI.nSP = sSPInfo.Num()-1;
      sSGI.AddElem(SGI);
    }
    else
    {
      SDynTexture2 *pNewTex = (SDynTexture2 *)(*ppTex);
      CTexture *pT = (CTexture *)pNewTex->GetTexture();
      pT->m_nAccessFrameID = m_nFrameUpdateID;
      const uint32 dwWidth = pT->GetWidth();
      const uint32 dwHeight = pT->GetHeight();
      pSP->m_pTex = pNewTex;
      uint32 nX, nY, nW, nH;
      pNewTex->GetSubImageRect(nX, nY, nW, nH);

      pSP->m_ucTexCoordMinX = (nX*128)/dwWidth;					assert((pSP->m_ucTexCoordMinX*dwWidth)/128==nX);
      pSP->m_ucTexCoordMinY = (nY*128)/dwHeight;				assert((pSP->m_ucTexCoordMinY*dwHeight)/128==nY);
      pSP->m_ucTexCoordMaxX = ((nX+nW)*128)/dwWidth;		assert((pSP->m_ucTexCoordMaxX*dwWidth)/128==nX+nW);
      pSP->m_ucTexCoordMaxY = ((nY+nH)*128)/dwHeight;		assert((pSP->m_ucTexCoordMaxY*dwHeight)/128==nY+nH);
    }

    pSP->m_fScaleV = fSpriteScaleV;

    // pass alpha test ref value
    pSP->m_Color.bcolor[0] = vi.ucSpriteAlphaTestRef;
		pSP->m_Color.bcolor[1] = vegetGroup.bUseTerrainColor ? o->m_ucSunDotTerrain : 255;

    //		float indirLightFactor = 1.f;

    uint32 dwIndirectLightingFactor = 0xff;		// [0..0xff]
    pSP->m_Color.bcolor[2] = dwIndirectLightingFactor;		// red channel used to darken only ambient level

    //		pSP->m_Color.bcolor[3] = 
    //			(byte)((fSunSkyRel * AmbientValue + fSkySunRel) * 
    //			(float)pLightInfo->m_BrightnessMultiplier);//cannot move that calc into m_BrightnessMultiplier since it depends on sprite itself

    //cannot move that calc into m_BrightnessMultiplier since it depends on sprite itself
    pSP->m_Color.bcolor[3] = (uint8)((((fAmbientValue*255))*pLightInfo->m_BrightnessMultiplier)/256);
    //		pSP->m_Color.bcolor[3] = (dwAmbientValue*(dwSunSkyRel+dwSkySunRel)*pLightInfo->m_BrightnessMultiplier)/(256*256);

    // test - not needed
    //		pSP->m_Color.bcolor[1] = dwAmbientValue;
    //		pSP->m_Color.bcolor[0] = ((dwSunSkyRel + dwSkySunRel)*pLightInfo->m_BrightnessMultiplier)/256;
  }

  if (sSGI.Num())
    MakeSprites(sSGI);

  if(!CV_r_VegetationSpritesGenAlways)
    if(sSGI.Num()>100 && s_dwOldMakeSpriteCalls>100)
      GetISystem()->GetILog()->LogWarning("MakeSprite() was called %d times in this frame and %d last frame (quick player view change/fast moving sun/too many sprites/low dynamic texture memory)",sSGI.Num(), s_dwOldMakeSpriteCalls);

  if((e_debug_mask & 0x20)!=0)
    GetISystem()->GetILog()->Log("MakeSprite() was called %d times in this frame (%d objects)",sSGI.Num(),pList->Count());

  s_dwOldMakeSpriteCalls = sSGI.Num();

  //::Sort(&sSPInfo[0], sSPInfo.Num());
  if((e_debug_mask & 0x10)==0)
  {
    PROFILE_FRAME(Generate_ObjSpritesList_Sort);
    std::sort(sSPInfo.begin(), sSPInfo.end(), CompareSpriteItem);
  }

	CV_r_PolygonMode = nPolygonMode;
  CV_r_usezpass = nZPass;
  CV_r_measureoverdraw = nMO;
}


//static CObjManager *sObjMan;

/*
static _inline int Compare(CVegetation *& p1, CVegetation *& p2)
{
  CStatObj * pStatObj1 = sObjMan->m_lstStaticTypes[p1->m_nObjectTypeID].GetStatObj();
  CStatObj * pStatObj2 = sObjMan->m_lstStaticTypes[p2->m_nObjectTypeID].GetStatObj();
  if((UINT_PTR)p1 > (UINT_PTR)p2)
    return 1;
  else
  if((UINT_PTR)p1 < (UINT_PTR)p2)
    return -1;
  
  return 0;
}
*/
void CD3D9Renderer::ObjSpritesFlush (TArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F>& Verts, TArray<ushort>& Inds, void *&pCurVB, SShaderTechnique *pTech)
{
  int nPointState = CTexture::GetTexState(STexState(FILTER_POINT, true));

  uint i;
  int nOffs, nIOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vDst = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(Verts.Num(), nOffs);
  if (!vDst)
  {
    Verts.SetUse(0);
    Inds.SetUse(0);
    return;
  }
  memcpy(vDst, &Verts[0], sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F)*Verts.Num());
  UnlockVB();

  ushort *iDst = GetIBPtr(Inds.Num(), nIOffs);
  if (!iDst)
  {
    Verts.SetUse(0);
    Inds.SetUse(0);
    return;
  }
  memcpy(iDst, &Inds[0], sizeof(ushort)*Inds.Num());
  UnlockIB();

  if (pCurVB != m_pVB[0])
  {
    pCurVB = m_pVB[0];
    FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  }

  m_RP.m_PersFlags |= RBPF_MULTILIGHTS;

	static ICVar * p_e_shadows = iConsole->GetCVar("e_shadows");

  bool bSunShadowExist = false;
  if(m_RP.m_DLights[SRendItem::m_RecurseLevel].Num())
    if( m_RP.m_DLights[SRendItem::m_RecurseLevel][0]->m_Flags & DLF_SUN )
      if( m_RP.m_DLights[SRendItem::m_RecurseLevel][0]->m_Flags & DLF_CASTSHADOW_MAPS )
        if(p_e_shadows->GetIVal())
          bSunShadowExist = true;

  //set shadow mask texture
	if (bSunShadowExist)
  {
    CTexture *pTexMask = CTexture::m_Text_BackBuffer;
    pTexMask->Apply(1, nPointState);
  }
	else 
	{
		CTexture::m_Text_Black->Apply( 1, nPointState);
	}

  if( m_RP.m_nAOMaskUpdateLastFrameId == GetFrameID(false) )
	{
		CTexture::m_Text_AOTarget->Apply(2, nPointState);
	}
	else 
	{
		CTexture::m_Text_White->Apply(2, nPointState);
	}

  SShaderPass *pPass = &pTech->m_Passes[0];
  for (i=0; i<pTech->m_Passes.Num(); i++, pPass++)
  {
    m_RP.m_pCurPass = pPass;
    if (!pPass->m_VShader || !pPass->m_PShader)
      continue;
    CHWShader *curVS = pPass->m_VShader;
    CHWShader *curPS = pPass->m_PShader;

    curPS->mfSet(0);
    curPS->mfSetParameters(1);

    curVS->mfSet(0);
    curVS->mfSetParameters(1);

    EF_Commit();

    int nPolys = Inds.Num()/3;
#if defined (DIRECT3D9) || defined (OPENGL)
    //m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, nOffs, nPolys);
    m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, nOffs, 0, Verts.Num(), nIOffs, nPolys);
#elif defined (DIRECT3D10)
    if (CHWShader_D3D::m_pCurInstVS && !CHWShader_D3D::m_pCurInstVS->m_bFallback && CHWShader_D3D::m_pCurInstPS && !CHWShader_D3D::m_pCurInstPS->m_bFallback)
    {
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      m_pd3dDevice->DrawIndexed(Inds.Num(), nIOffs, nOffs);
    }
#endif
    m_RP.m_PS.m_NumSpriteDIPS++;
    m_RP.m_PS.m_NumSpritePolys += nPolys;
  }
  m_RP.m_PersFlags &= ~RBPF_MULTILIGHTS;

  Verts.SetUse(0);
  Inds.SetUse(0);
}
/*
static _inline int Compare(SSpriteInfo& p1, SSpriteInfo& p2)
{
  if(p1.m_TexID > p2.m_TexID)
    return 1;
  else
  if(p1.m_TexID < p2.m_TexID)
    return -1;
  if (p1.m_nLightMask < p2.m_nLightMask)
    return -1;
  if (p1.m_nLightMask > p2.m_nLightMask)
    return -1;

  return 0;
}
*/

extern CTexture *gTexture2;

void CD3D9Renderer::DrawObjSprites_NoBend_Merge_Technique (TArray<SSpriteInfo>& SPInfo, bool bZ, bool bShadows)
{
  uint i;

  CTexture *pPrevTex = NULL;
	SSectorTextureSet * pPrevTerrainTexInfo = NULL;

  CTexture::BindNULLFrom(1);
  int nState;
  int nAlphaRef = 0;
  if(CV_r_VegetationSpritesAlphaBlend)
  {
    nState = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_ALPHATEST_GREATER;
    nAlphaRef = 0;
  }
  else
  {
    if(m_RP.m_PersFlags2 & RBPF2_IMPOSTERGEN)
      nState = GS_DEPTHWRITE;
    else
    {
      nState = GS_DEPTHWRITE;
      //nAlphaRef = 40;
    }
  }
  FX_ZState(nState);
  if (bShadows)
  {
    int nColorMask = ((~(1<<0))<<GS_COLMASK_SHIFT) & GS_COLMASK_MASK ;
    nState |= nColorMask | GS_BLSRC_ONE | GS_BLDST_ONE;
  }
  EF_SetState(nState, nAlphaRef);

  D3DSetCull(eCULL_None);

  m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];
  SRenderShaderResources rs;
  SRenderShaderResources *pResSave = m_RP.m_pShaderResources;
  m_RP.m_pShaderResources = &rs;
  rs.m_AlphaRef = (float)nAlphaRef / 255.0f;

  m_RP.m_PersFlags &= ~RBPF_USESTREAM_MASK;

  m_RP.m_FlagsStreams_Decl = 0;
  m_RP.m_FlagsStreams_Stream = 0;

  int nf = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
  m_RP.m_CurVFormat = nf;
  m_RP.m_FlagsShader_MD = 0;
  m_RP.m_FlagsShader_MDV = 0;
  m_RP.m_FlagsShader_LT = 0;

  if (!CV_r_usezpass) 
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NOZPASS];

  static TArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> sVerts;
  sVerts.SetUse(0);
  static TArray<ushort> sInds;
  sInds.SetUse(0);

  m_cEF.m_ShaderTreeSprites->FXBegin(&m_RP.m_nNumRendPasses, bShadows ? FEF_DONTSETSTATES : FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  m_cEF.m_ShaderTreeSprites->FXBeginPass(0);

  m_RP.m_CurVFormat = nf;
  HRESULT hr = EF_SetVertexDeclaration(m_RP.m_FlagsStreams_Decl&7, m_RP.m_CurVFormat);
  if (hr != S_OK)
    return;
  FX_SetIStream(m_pIB);

  // set atlas size of custom texture filter
  static CCryName TexAtlasSizeName("TexAtlasSize");
  const Vec4 cFloatVal(CV_r_texatlassize, CV_r_texatlassize, 1.f/CV_r_texatlassize, 1.f/CV_r_texatlassize);
  m_cEF.m_ShaderTreeSprites->FXSetPSFloat(TexAtlasSizeName, &cFloatVal, 1);

  void *pCurVB = NULL;

  bool bVolFog = false;
  D3DXMATRIX mat0, mat1;
  D3DXMATRIX mat;
  ColorF FogColor;

  /*static D3DSurface *pSurf = NULL;
  if (!pSurf)
  {
    hr = m_pd3dDevice->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pSurf, NULL);
  }

  if (gDT)
  {
    int nFrame = m_nFrameSwapID;
    if ((1<<(nFrame&1)) & gnFrame)
    {
      SDynTexture2 *pDT = gDT;
      pDT->Apply(0);
      FX_PushRenderTarget(0, pSurf, &m_DepthBufferOrig);
      EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
      DrawFullScreenQuad(m_cEF.m_ShaderTreeSprites, "General_Debug", m_cEF.m_RTRect.x, m_cEF.m_RTRect.y, m_cEF.m_RTRect.x+m_cEF.m_RTRect.z, m_cEF.m_RTRect.y+m_cEF.m_RTRect.w);
      FX_PopRenderTarget(0);
      D3DLOCKED_RECT lr;
      pSurf->LockRect(&lr, NULL, 0);
      UCol *pData = (UCol *)lr.pBits;
      //for (int i=0; i<64*64; i++)
      //{
      UCol d = pData[32*64+32];
      if (d.bcolor[0] == 0xff)
      {
        //assert(0);
        int nnn = 0;
      }
      //}
      pSurf->UnlockRect();
      m_cEF.m_ShaderTreeSprites->FXSetTechnique("General");
      m_cEF.m_ShaderTreeSprites->FXBegin(&m_RP.m_nNumRendPasses, bShadows ? FEF_DONTSETSTATES : FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      m_cEF.m_ShaderTreeSprites->FXBeginPass(0);
      m_RP.m_CurVFormat = nf;

      hr = EF_SetVertexDeclaration(m_RP.m_FlagsStreams_Decl&7, m_RP.m_CurVFormat);
      if (hr != S_OK)
        return;
      FX_SetIStream(m_pIB);
      pCurVB = NULL;

      // set atlas size of custom texture filter
      static CCryName TexAtlasSizeName("TexAtlasSize");
      const Vec4 cFloatVal(CV_r_texatlassize, CV_r_texatlassize, 1.f/CV_r_texatlassize, 1.f/CV_r_texatlassize);
      m_cEF.m_ShaderTreeSprites->FXSetPSFloat(TexAtlasSizeName, &cFloatVal, 1);
      EF_SetState(nState, nAlphaRef);

    }
  }*/

  int nPointState = CTexture::GetTexState(STexState(FILTER_POINT, true));

  uint nPrevLMask = (uint)-1;
  int nPasses = 0;
  for (i=0; i<(int)SPInfo.Num(); i++)
  {
    SSpriteInfo *pSP = &SPInfo[i];

    SDynTexture2 *pDT = (SDynTexture2 *)pSP->m_pTex;
    if (!pDT)
      continue;
    CTexture *pTex = pDT->m_pTexture;
    // Make sure we updated the texture on this frame
		if(CRenderer::IsMultiGPUModeActive())
    {
      assert(pDT->IsValid());
    }
    if (!pTex)
      continue;
    /*{
      pDT->Apply(0);
      if (!strcmp(pDT->m_sSource, "$SpriteTex_(1049 2070 249)_1_8"))
      {
        if (!gDT)
        {
          gDT = pDT;

          gnFrame = pDT->m_nUpdateMask;
        }
        int nnn = 0;
      }
      FX_PushRenderTarget(0, pSurf, &m_DepthBufferOrig);
      EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
      DrawFullScreenQuad(m_cEF.m_ShaderTreeSprites, "General_Debug", m_cEF.m_RTRect.x, m_cEF.m_RTRect.y, m_cEF.m_RTRect.x+m_cEF.m_RTRect.z, m_cEF.m_RTRect.y+m_cEF.m_RTRect.w);
      FX_PopRenderTarget(0);
      D3DLOCKED_RECT lr;
      pSurf->LockRect(&lr, NULL, 0);
      UCol *pData = (UCol *)lr.pBits;
      //for (int i=0; i<64*64; i++)
      //{
        UCol d = pData[32*64+32];
        if (d.bcolor[0] == 0xff)
        {
          //assert(0);
          int nnn = 0;
        }
      //}
      pSurf->UnlockRect();
      m_cEF.m_ShaderTreeSprites->FXSetTechnique("General");
      m_cEF.m_ShaderTreeSprites->FXBegin(&m_RP.m_nNumRendPasses, bShadows ? FEF_DONTSETSTATES : FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      m_cEF.m_ShaderTreeSprites->FXBeginPass(0);
      m_RP.m_CurVFormat = nf;

      hr = EF_SetVertexDeclaration(m_RP.m_FlagsStreams_Decl&7, m_RP.m_CurVFormat);
      if (hr != S_OK)
        return;
      FX_SetIStream(m_pIB);
      pCurVB = NULL;

      // set atlas size of custom texture filter
      static CCryName TexAtlasSizeName("TexAtlasSize");
      const Vec4 cFloatVal(CV_r_texatlassize, CV_r_texatlassize, 1.f/CV_r_texatlassize, 1.f/CV_r_texatlassize);
      m_cEF.m_ShaderTreeSprites->FXSetPSFloat(TexAtlasSizeName, &cFloatVal, 1);
      EF_SetState(nState, nAlphaRef);
      //pTex->Apply(0, nPointState);
    }*/
		if(	pPrevTex != pTex || pPrevTerrainTexInfo != pSP->m_pTerrainTexInfo )
    {
      if (sVerts.Num())
        ObjSpritesFlush(sVerts, sInds, pCurVB, m_RP.m_pCurTechnique);
      if(!bShadows)
			{
        gTexture2 = pTex;
				pTex->Apply(0, nPointState);

				if(m_RP.m_nPassGroupID == EFSLIST_GENERAL && pSP->m_pTerrainTexInfo)
				{
          SSectorTextureSet * pTexInfo = pSP->m_pTerrainTexInfo;

					// set new RT flags
					m_cEF.m_ShaderTreeSprites->FXEndPass();
					m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

					m_cEF.m_ShaderTreeSprites->FXBeginPass(0);

					CTexture * pTerrTex = CTexture::GetByID(pTexInfo->nTex0);
					pTerrTex->Apply(3);

					static CCryName SpritesOutdoorAOVertInfoName("SpritesOutdoorAOVertInfo");
					const Vec4 cFloatVal(pTexInfo->fTexOffsetX, pTexInfo->fTexOffsetY, pTexInfo->fTexScale, pTexInfo->fTerrainMinZ);
					m_cEF.m_ShaderTreeSprites->FXSetVSFloat(SpritesOutdoorAOVertInfoName, &cFloatVal, 1);
				}
        else
          m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];


        { // set RT_AMBIENT if no light sources present, used by blend with terrain color feature
          bool bSunExist = false;
          if(m_RP.m_DLights[SRendItem::m_RecurseLevel].Num())
            if( m_RP.m_DLights[SRendItem::m_RecurseLevel][0]->m_Flags & DLF_SUN )
              bSunExist = true;

          if(!bSunExist)
            m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_AMBIENT];
          else
            m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_AMBIENT];
        }
			}
			pPrevTex = pTex;
			pPrevTerrainTexInfo = pSP->m_pTerrainTexInfo;
    }

    int nOffs = sVerts.Num();
    int nIOffs = sInds.Num();
    if (nOffs+4 >= MAX_DYNVB3D_VERTS)
    {
      ObjSpritesFlush(sVerts, sInds, pCurVB, m_RP.m_pCurTechnique);
      nOffs = sVerts.Num();
      nIOffs = sInds.Num();
    }
    sVerts.AddIndex(4);
    sInds.AddIndex(6);
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pQuad = &sVerts[nOffs];

		const float fByte2TexMul = 1.0f/128.0f;

		ITexture *pITexture = pDT->GetTexture();

		float fHalfInvWidth = 0.5f/pITexture->GetWidth();
		float fHalfInvHeight = 0.5f/pITexture->GetHeight();

    float fX0 = pSP->m_vPos.x+pSP->m_fDX;
    float fX1 = pSP->m_vPos.x-pSP->m_fDX;
    float fY0 = pSP->m_vPos.y+pSP->m_fDY;
    float fY1 = pSP->m_vPos.y-pSP->m_fDY;

    Vec3 vUp = Vec3(0,0,-pSP->m_fScaleV);

    if(pSP->m_AngleY)
      vUp = vUp.GetRotated(Vec3(-pSP->m_fDX,pSP->m_fDY,0).normalized(), DEG2RAD(pSP->m_AngleY));

    uint dCol = pSP->m_Color.dcolor;
    float fST00 = pSP->m_ucTexCoordMaxX*fByte2TexMul-fHalfInvWidth;
    float fST01 = pSP->m_ucTexCoordMaxY*fByte2TexMul-fHalfInvHeight;
    float fST10 = pSP->m_ucTexCoordMinX*fByte2TexMul+fHalfInvWidth;
    float fST11 = pSP->m_ucTexCoordMinY*fByte2TexMul+fHalfInvHeight;

    // 0,1 verts
    pQuad[0].xyz.x = fX0+vUp.x;
    pQuad[0].xyz.y = fY1+vUp.y;
    pQuad[0].xyz.z = pSP->m_vPos.z+vUp.z;
    pQuad[0].color.dcolor = dCol;
    pQuad[0].st[0] = fST00;
    pQuad[0].st[1] = fST01;

    pQuad[1].xyz.x = fX1+vUp.x;
    pQuad[1].xyz.y = fY0+vUp.y;
    pQuad[1].xyz.z = pSP->m_vPos.z+vUp.z;
    pQuad[1].color.dcolor = dCol;
    pQuad[1].st[0] = fST10;
		pQuad[1].st[1] = fST01;

    // 2,3 verts
    pQuad[2].xyz.x = fX0-vUp.x;
    pQuad[2].xyz.y = fY1-vUp.y;
    pQuad[2].xyz.z = pSP->m_vPos.z-vUp.z;
    pQuad[2].color.dcolor = dCol;
    pQuad[2].st[0] = fST00;
    pQuad[2].st[1] = fST11;

    pQuad[3].xyz.x = fX1-vUp.x;
    pQuad[3].xyz.y = fY0-vUp.y;
    pQuad[3].xyz.z = pSP->m_vPos.z-vUp.z;
    pQuad[3].color.dcolor = dCol;
    pQuad[3].st[0] = fST10;
    pQuad[3].st[1] = fST11;

    ushort *pInds = &sInds[nIOffs];
    pInds[0] = nOffs;
    pInds[1] = nOffs+1;
    pInds[2] = nOffs+2;
    pInds[3] = nOffs+1;
    pInds[4] = nOffs+2;
    pInds[5] = nOffs+3;
  }

  if (sVerts.Num())
    ObjSpritesFlush(sVerts, sInds, pCurVB, m_RP.m_pCurTechnique);
  else
    EF_Commit();

  CTexture::BindNULLFrom(0);

  m_RP.m_pShaderResources = pResSave;
}


void CD3D9Renderer::DrawSpritesShadows(bool& bBlur, bool& bSetRT, CTexture*& tpSrc)
{
	//sun light only
	int nGroup( 0 );
	int i( 0 );
	int nLightID( i + nGroup * 4 );

	if( /*!m_RP.m_pSunLight || */SRendItem::m_RecurseLevel > 1 )
		return;

	if( !m_RP.m_DLights[ SRendItem::m_RecurseLevel ].Num() )
		return;

	CDLight* pLight( m_RP.m_DLights[ SRendItem::m_RecurseLevel ][ nLightID ] );
	if( !( pLight->m_Flags & DLF_SUN ) )
		return;

	ShadowMapFrustum** ppSMFrustumList = pLight->m_pShadowMapFrustums;
	//assert( 0 != ppSMFrustumList );
	if (!ppSMFrustumList)
		return;

  /*if (!bSetRT)
  {
    bBlur = FX_SetShadowMaskRT(0, pGr, tpSrc);
    bSetRT = true;  
  }*/

	//init shadow casters
	int nCasters( 0 );

	for( int i=0; i<MAX_GSM_LODS_NUM; i++ )
	{
		if(ppSMFrustumList[i] && ppSMFrustumList[i]->pCastersList->Count())
		{
			nCasters++;
			//PrepareDepthMap(ppSMFrustumList[ i ]);
		}
	}

	//no sun frustums
	if( nCasters == 0)
		return;

	//reset frustum list 
	ppSMFrustumList = pLight->m_pShadowMapFrustums;

	// set global shader RT flags
	if( CV_r_shadow_jittering > 0.0f )
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_JITTERING ];

	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_MIXED_MAP_G16R16 ];


	// set global shader RT flags
	if( CV_r_shadow_jittering > 0.0f )
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_JITTERING ];

//	static ICVar* p_e_shadows_tsm( iConsole->GetCVar( "e_shadows_tsm" ) ); //TODO: move vars to renderer
	//if( p_e_shadows_tsm->GetIVal() > 0 )
		//m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_PSM ]; //enable PSM

	// set pass dependent RT flags
	m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[ HWSR_SAMPLE0 ] | g_HWSR_MaskBit[ HWSR_SAMPLE1 ] | g_HWSR_MaskBit[ HWSR_SAMPLE2 ] | g_HWSR_MaskBit[ HWSR_SAMPLE3 ] );
	if( nCasters >= 1 )
	{
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE0 ];
		if( nCasters >= 2 )
		{
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE1 ];
			if( nCasters >= 3 )
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE2 ];
				if( nCasters >= 4 )
					m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE3 ];
			}
		}
	}

	// setup projection data (texture, matrices, etc.)
	int nSamplerId = 0;
	for( int j = 0; j < MAX_GSM_LODS_NUM; ++j )
	{
		if(ppSMFrustumList[j] && ppSMFrustumList[j]->pCastersList->Count())
		{
      if (ppSMFrustumList[j]->bHWPCFCompare)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ];
			SetupShadowOnlyPass( nSamplerId, ppSMFrustumList[ j ], 0 );
			nSamplerId++;
		}
	}

	//adjust bias
	m_cEF.m_TempVecs[1] += Vec4(0.01f,0.01f,0.01f,0.01f);

	//FX_PushRenderTarget(0, CTexture::m_Text_ScreenShadowMap[0], &CTexture::m_DepthBufferOrig);
	m_cEF.m_ShaderTreeSprites->FXSetTechnique("ShadowPass");
	DrawObjSprites_NoBend_Merge_Technique(sSPInfo, false, true);
	//FX_PopRenderTarget(0);
}

void CD3D9Renderer::GenerateObjSprites (PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)
{
  PROFILE_FRAME(Generate_ObjSpritesList);

  GenerateSpritesList(pList, fMaxViewDist, pObjMan, fZoomFactor);
}

void CD3D9Renderer::DrawObjSprites (PodArray<SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)
{
  if (SRendItem::m_RecurseLevel != 1)
    return;

  if (m_RP.m_nBatchFilter & FB_Z)
  {
    if (CV_r_usezpass && !(m_RP.m_PersFlags2 & RBPF2_IMPOSTERGEN))
    {
      static CCryName TechName("General_Z");
      m_cEF.m_ShaderTreeSprites->FXSetTechnique(TechName);
			{
				PROFILE_FRAME(Draw_ObjSprites_Z);
	      DrawObjSprites_NoBend_Merge_Technique(sSPInfo, true, false);
			}
    }
  }
  else
  {
    //DrawSpritesShadows();
    static CCryName TechName("General");
    m_cEF.m_ShaderTreeSprites->FXSetTechnique(TechName);
		{
			PROFILE_FRAME(Draw_ObjSprites);
	    DrawObjSprites_NoBend_Merge_Technique(sSPInfo, false, false);
		}
  }
}

uint CD3D9Renderer::RenderOccludersIntoBuffer(const CCamera & viewCam, 
																							 int nTexSize, 
																							 PodArray<IRenderNode*> & lstOccluders,
																							 float * pDst)
{
	PROFILE_FRAME(RenderOccludersIntoBuffer);

	int nOldNumDrawCalls = m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP];

	if (CV_r_flush == 3)
		FlushHardware(true);

	SetCamera(viewCam);

	static SDynTexture * pDynTexture = new SDynTexture(nTexSize, nTexSize, eTF_R32F, eTT_2D, 0, "RenderOccludersIntoBuffer");
	if(pDynTexture->GetWidth() != nTexSize)
	{
		pDynTexture->Release();
		pDynTexture = new SDynTexture(nTexSize, nTexSize, eTF_R32F, eTT_2D, 0, "RenderOccludersIntoBuffer");
	}

	pDynTexture->SetRT(0, true, FX_GetDepthSurface(nTexSize, nTexSize, false));

	m_cEF.m_TempVecs[0].x = viewCam.GetNearPlane();
	m_cEF.m_TempVecs[0].y = viewCam.GetFarPlane();
	m_cEF.m_TempVecs[0].z = 0;
	m_cEF.m_TempVecs[0].w = 1;

	// render object
	ColorF white(1,1,1,1);
	EF_ClearBuffers(FRT_CLEAR, &white);

	Vec4 vZRange;
	vZRange.x = -viewCam.GetNearPlane() / (viewCam.GetFarPlane() - viewCam.GetNearPlane());
	vZRange.y = 1.0f / (viewCam.GetFarPlane() - viewCam.GetNearPlane());
	vZRange.z = 1;
	vZRange.w = 0;
	EF_Commit();

	EF_StartEf();  

	int old_m_polygon_mode = m_polygon_mode;
	SetPolygonMode(R_SOLID_MODE);
	int oldCV_r_showlines = CV_r_showlines;
	CV_r_showlines = 0;

	for(int i=0; i<lstOccluders.Count(); i++)
	{
		SRendParams rParms;
		rParms.nTechniqueID = TTYPE_SHADOWGEN; 
		rParms.dwFObjFlags |= FOB_TRANS_MASK | FOB_RENDER_INTO_SHADOWMAP; 
		lstOccluders[i]->Render(rParms);
	}

	EF_EndEf3D(0);

	pDynTexture->RestoreRT(0,true);

	EF_Commit();

#if defined (DIRECT3D9) || defined (OPENGL)
	// read target texture into system memory
	LPDIRECT3DTEXTURE9 pID3DTextureDST = NULL;
	LPDIRECT3DSURFACE9 pSurfDST, pSurfSRC;
	HRESULT h = S_OK;
	h = D3DXCreateTexture(m_pd3dDevice, nTexSize, nTexSize, 1, 0, D3DFMT_R32F, D3DPOOL_SYSTEMMEM, &pID3DTextureDST);
	assert(SUCCEEDED(h));
	h = pID3DTextureDST->GetSurfaceLevel(0, &pSurfDST);
	IDirect3DTexture9 *pTexSrc = (IDirect3DTexture9 *)pDynTexture->m_pTexture->GetDeviceTexture();
	h = pTexSrc->GetSurfaceLevel(0, &pSurfSRC);
	m_pd3dDevice->GetRenderTargetData(pSurfSRC, pSurfDST);
	SAFE_RELEASE(pSurfSRC);
	D3DLOCKED_RECT rc;
	pSurfDST->LockRect(&rc, NULL, D3DLOCK_READONLY);
	assert(nTexSize == rc.Pitch/4);
	float * pSrc = (float*)rc.pBits;

	memset(pDst, 0, nTexSize*nTexSize*sizeof(float));

	// convert into coverage buffer format
	float fFrustumRange = viewCam.GetFarPlane()-viewCam.GetNearPlane();
	for(int x=0; x<nTexSize; x++)
	{
		for(int y=0; y<nTexSize; y++)
		{
			float fDist = pSrc[x*nTexSize + y] * fFrustumRange;
			if(fDist<0.001f)
				fDist = 0.001f;
			else if(fDist>fFrustumRange)
				fDist = fFrustumRange;

			fDist = 1.f/fDist;

			if(pDst[y + (nTexSize-1-x)*nTexSize] < fDist)
				pDst[y + (nTexSize-1-x)*nTexSize] = fDist;

			if(y>0 && fDist>pDst[(y-1) + (nTexSize-1-x)*nTexSize])
				pDst[(y-1) + (nTexSize-1-x)*nTexSize] = fDist;

			if(x<nTexSize-1 && fDist>pDst[(y) + (nTexSize-1-x+1)*nTexSize])
				pDst[(y) + (nTexSize-1-x+1)*nTexSize] = fDist;
		}
	}

	pSurfDST->UnlockRect();
	SAFE_RELEASE(pSurfDST);
	SAFE_RELEASE(pID3DTextureDST);
#endif

	SetPolygonMode(old_m_polygon_mode);
	CV_r_showlines = oldCV_r_showlines;

	m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP] = nOldNumDrawCalls;

	return 1;
}
