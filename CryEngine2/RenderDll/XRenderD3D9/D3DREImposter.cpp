/*=============================================================================
  D3DREImposter.cpp : D3D specific depth imposters rendering.
  Copyright 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"

//=======================================================================

CTexture *gTexture;
CTexture *gTexture2;
IDynTexture *CREImposter::m_pScreenTexture;

int IntersectRayAABB(Vec3 p, Vec3 d, SMinMaxBox a, Vec3& q);


static void GetEdgeNo( const uint32 dwEdgeNo, uint32 &dwA, uint32 &dwB )
{
	switch(dwEdgeNo)
	{
		case  0:	dwA=0;dwB=1;break;
		case  1:	dwA=2;dwB=3;break;
		case  2:	dwA=4;dwB=5;break;
		case  3:	dwA=6;dwB=7;break;

		case  4:	dwA=0;dwB=2;break;
		case  5:	dwA=4;dwB=6;break;
		case  6:	dwA=5;dwB=7;break;
		case  7:	dwA=1;dwB=3;break;

		case  8:	dwA=0;dwB=4;break;
		case  9:	dwA=2;dwB=6;break;
		case 10:	dwA=3;dwB=7;break;
		case 11:	dwA=1;dwB=5;break;

		default: assert(0);
	}
}



bool CREImposter::PrepareForUpdate()
{
  CD3D9Renderer *rd = gcpRendD3D;
  CRenderCamera cam = rd->GetRCamera();
  int i;

  if (SRendItem::m_RecurseLevel > 1)
    return false;


	float fMinX, fMaxX, fMinY, fMaxY;
	uint32 dwBestEdge = 0xffffffff;		// favor the last edge we found
	float fBestArea = FLT_MAX;

  Vec3 vCenter = GetPosition();
  Vec3 vEye = vCenter - cam.Orig;
  float fDistance  = vEye.GetLength();
  vEye /= fDistance;

  int32 D3DVP[4];
	rd->GetViewport(&D3DVP[0], &D3DVP[1], &D3DVP[2], &D3DVP[3]);

  Vec3 vProjPos[9];
  Vec3 vUnProjPos[9];
  for (i=0; i<8; i++)
  {
    if (i & 1)
      vUnProjPos[i].x = m_WorldSpaceBV.GetMax().x;
    else
      vUnProjPos[i].x = m_WorldSpaceBV.GetMin().x;

    if (i & 2)
      vUnProjPos[i].y = m_WorldSpaceBV.GetMax().y;
    else
      vUnProjPos[i].y = m_WorldSpaceBV.GetMin().y;

    if (i & 4)
      vUnProjPos[i].z = m_WorldSpaceBV.GetMax().z;
    else
      vUnProjPos[i].z = m_WorldSpaceBV.GetMin().z;
  }

  vUnProjPos[8] = vCenter;
/*
	// test
	rd->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	rd->GetIRenderAuxGeom()->DrawAABB(AABB(m_WorldSpaceBV.GetMin(),m_WorldSpaceBV.GetMax()),false,ColorB(0,255,255,255),eBBD_Faceted);
*/

	CRenderCamera tempCam;
	Matrix44 viewMat, projMat;

	tempCam.Near=cam.Near;
	tempCam.Far=cam.Far;

	mathMatrixPerspectiveOffCenter(&projMat,-1,1,1,-1,tempCam.Near,tempCam.Far);

	float fOldEdgeArea = -FLT_MAX;

	// try to find minimal enclosing rectangle assuming the best projection frustum must be aligned to a AABB edge
	for(uint32 dwEdge=0;dwEdge<13;++dwEdge)		// 12 edges and iteration no 13 processes the best again
	{
		uint32 dwEdgeA, dwEdgeB;

		if(dwEdge==12 && fBestArea>fOldEdgeArea*0.98f)		// not a lot better than old axis then keep old axis (to avoid jittering)
			dwBestEdge=m_nLastBestEdge;

		if(dwEdge==12)
			GetEdgeNo(dwBestEdge,dwEdgeA,dwEdgeB);		// the best again
		 else
			GetEdgeNo(dwEdge,dwEdgeA,dwEdgeB);				// edge no dwEdge

		Vec3 vEdge[2]={ vUnProjPos[dwEdgeA], vUnProjPos[dwEdgeB]};
/*
		if(dwEdge==12)
		{
			// axis that allows smallest enclosing rectangle
			rd->GetIRenderAuxGeom()->DrawLine( vEdge[0],ColorB(0,0,255,255),vEdge[1],ColorB(0,255,255,255),10.0f);	
		}
*/

		Vec3 vR = vEdge[0]-vEdge[1];
		Vec3 vU = (vEdge[0]-cam.Orig) ^ vR;

		tempCam.LookAt(cam.Orig, vCenter, vU);
		tempCam.GetModelviewMatrix(viewMat.GetData());
		mathVec3ProjectArray((Vec3 *)&vProjPos[0].x, sizeof(Vec3), (Vec3 *)&vUnProjPos[0].x, sizeof(Vec3), D3DVP, &projMat, &viewMat, &sIdentityMatrix, 9, g_CpuFlags);

		// Calculate 2D extents
		fMinX = fMinY = FLT_MAX;
		fMaxX = fMaxY = -FLT_MAX;
		for (i=0; i<8; i++)
		{
			if (fMinX > vProjPos[i].x)
				fMinX = vProjPos[i].x;
			if (fMinY > vProjPos[i].y)
				fMinY = vProjPos[i].y;

			if (fMaxX < vProjPos[i].x)
				fMaxX = vProjPos[i].x;
			if (fMaxY < vProjPos[i].y)
				fMaxY = vProjPos[i].y;
		}

		float fArea = (fMaxX-fMinX)*(fMaxY-fMinY);

		if(dwEdge==m_nLastBestEdge)
			fOldEdgeArea=fArea;

		if(fArea<fBestArea)
		{
			dwBestEdge=dwEdge;
			fBestArea=fArea;
		}
	}

/*
	// low precision - jitters
  // LB, RB, RT, LT
  vProjPos[0] = Vec3(fMinX, fMinY, vProjPos[8].z);
  vProjPos[1] = Vec3(fMaxX, fMinY, vProjPos[8].z);
  vProjPos[2] = Vec3(fMaxX, fMaxY, vProjPos[8].z);
  vProjPos[3] = Vec3(fMinX, fMaxY, vProjPos[8].z);
  // Unproject back to the world with new z-value
  mathVec3UnprojectArray((Vec3 *)&vUnProjPos[0].x, sizeof(Vec3), (Vec3 *)&vProjPos[0].x, sizeof(Vec3), D3DVP, &projMat, &viewMat, &sIdentityMatrix, 4, g_CpuFlags);
*/
	// high precision - no jitter
	float fCamZ = (tempCam.Orig-vCenter).Dot(tempCam.ViewDir());
	float f = -fCamZ/tempCam.Near;
	vUnProjPos[0] = tempCam.CamToWorld(Vec3((fMinX/D3DVP[2]*2.0f-1.0f)*f,(fMinY/D3DVP[3]*2.0f-1.0f)*f,fCamZ));
	vUnProjPos[1] = tempCam.CamToWorld(Vec3((fMaxX/D3DVP[2]*2.0f-1.0f)*f,(fMinY/D3DVP[3]*2.0f-1.0f)*f,fCamZ));
	vUnProjPos[2] = tempCam.CamToWorld(Vec3((fMaxX/D3DVP[2]*2.0f-1.0f)*f,(fMaxY/D3DVP[3]*2.0f-1.0f)*f,fCamZ));
	vUnProjPos[3] = tempCam.CamToWorld(Vec3((fMinX/D3DVP[2]*2.0f-1.0f)*f,(fMaxY/D3DVP[3]*2.0f-1.0f)*f,fCamZ));



	// test
//	rd->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
//	rd->GetIRenderAuxGeom()->DrawPolyline(vUnProjPos,4,true,ColorB(0,0,255,0),20);

	m_vPos = vCenter;
  Vec3 vProjCenter = (vUnProjPos[0] + vUnProjPos[1] + vUnProjPos[2] + vUnProjPos[3]) / 4.0f;
  Vec3 vDif = vProjCenter - vCenter;
  float fDerivX = vDif * tempCam.X;
  float fDerivY = vDif * tempCam.Y;

  float fRadius = m_WorldSpaceBV.GetRadius();
  Vec3 vRight = vUnProjPos[0] - vUnProjPos[1];
  Vec3 vUp = vUnProjPos[0] - vUnProjPos[2];
  float fRadiusX = vRight.len() / 2 + fabsf(fDerivX);
  float fRadiusY = vUp.len() / 2 + fabsf(fDerivY);

  Vec3 vNearest;
  int nCollide = IntersectRayAABB(cam.Orig, vEye, m_WorldSpaceBV, vNearest);
  Vec4 v4Nearest = Vec4(vNearest, 1);
  Vec4 v4Far = Vec4(vNearest + vEye*fRadius*2, 1);
  Vec4 v4ZRange = Vec4(0,0,0,0);
  Vec4 v4Column2 = rd->m_CameraProjMatrix.GetColumn4(2);
  Vec4 v4Column3 = rd->m_CameraProjMatrix.GetColumn4(3);
  float fZ = v4Nearest.Dot(v4Column2);
  float fW = v4Nearest.Dot(v4Column3);

	bool bScreen = false;
	
	if(fabs(fW)<0.001f)				// to avoid division by 0 (near the object Screen is used and the value doesn't matter anyway)
	{
		m_fNear = 0.0f;
		bScreen=true;
	}
	else m_fNear = 0.999f * fZ / fW;		

  fZ = v4Far.Dot(v4Column2);
  fW = v4Far.Dot(v4Column3);

	if(fabs(fW)<0.001f)				// to avoid division by 0 (near the object Screen is used and the value doesn't matter anyway)
	{
		m_fFar = 1.0f;
		bScreen=true;
	}
	else m_fFar = fZ / fW;		

  float fCamRadiusX = sqrtf(cam.wR*cam.wR + cam.Near*cam.Near);
  float fCamRadiusY = sqrtf(cam.wT*cam.wT + cam.Near*cam.Near);

  float fWidth     = cam.wR - cam.wL;
  float fHeight    = cam.wT - cam.wB;
  
	if(!bScreen)
		bScreen = (fRadiusX*cam.Near/fDistance >= fWidth || fRadiusY*cam.Near/fDistance >= fHeight || (fDistance-fRadiusX <= fCamRadiusX) || (fDistance-fRadiusY <= fCamRadiusY));

	IDynTexture *pDT = bScreen ? m_pScreenTexture : m_pTexture;


	float fRequiredResX = 1024;
	float fRequiredResY = 512;

	float fTexScale = CRenderer::CV_r_imposterratio>0.1f ? 1.0f/CRenderer::CV_r_imposterratio : 1.0f/0.1f;

	if (!bScreen)  // outside cloud
	{
		assert(D3DVP[0]==0 && D3DVP[1]==0);			// otherwise the following lines don't make sense

		float fRadPixelX = (fMaxX-fMinX)*2;		// for some reason *2 is needed, most likely /near (*4) is the correct
		float fRadPixelY = (fMaxY-fMinY)*2;

		fRequiredResX = min(fRequiredResX,max(16.0f,fRadPixelX));
		fRequiredResY = min(fRequiredResY,max(16.0f,fRadPixelY));
	}

	int nRequiredLogXRes = LogBaseTwo((int)(fRequiredResX*fTexScale));
	int nRequiredLogYRes = LogBaseTwo((int)(fRequiredResY*fTexScale));

	if (IsImposterValid(cam, fRadiusX, fRadiusY, fCamRadiusX, fCamRadiusY,nRequiredLogXRes,nRequiredLogYRes,dwBestEdge))
  {
    if (!pDT || !pDT->IsValid()) 
      return true;

    if (!CRenderer::CV_r_cloudsupdatealways)
      return false;
  }
  if (pDT)
    pDT->ResetUpdateMask();

  bool bPostpone = false;
  int nCurFrame = rd->GetFrameID(false);
	if(!gRenDev->IsMultiGPUModeActive())
  {
    if (!CRenderer::CV_r_cloudsupdatealways && !bScreen && pDT && pDT->GetTexture() && m_fRadiusX && m_fRadiusY)
    {
      if (m_MemUpdated > CRenderer::CV_r_impostersupdateperframe)
        bPostpone = true;
      if (m_PrevMemPostponed)
      {
        int nDeltaFrames = m_PrevMemPostponed / CRenderer::CV_r_impostersupdateperframe;
        if (nCurFrame - m_FrameUpdate <= nDeltaFrames)
          bPostpone = true;
      }
      if (bPostpone)
      {
        m_MemPostponed += (1<<nRequiredLogXRes) * (1<<nRequiredLogYRes) * 4 / 1024;
        return false;
      }
    }
  }
  m_FrameUpdate = nCurFrame;

  Matrix44 M;

  CRenderObject *pInstObj = rd->m_RP.m_pCurObject;

  m_LastCamera = cam;
	m_vLastSunDir = gEnv->p3DEngine->GetSunDir().GetNormalized();

	m_nLogResolutionX = nRequiredLogXRes;
	m_nLogResolutionY = nRequiredLogYRes;

	if (!bScreen) 
  { // outside cloud
//    m_LastCamera.TightlyFitToSphere(cam.Orig, vU, m_vPos, fRadiusX, fRadiusY);

		m_LastCamera=tempCam;
/*
		float DistToCntr = (m_vPos-cam.Orig) | m_LastCamera.ViewDir();

		// are the following 2 lines correct ?
		m_LastCamera.Near = DistToCntr-fRadiusX;
		m_LastCamera.Far = DistToCntr+fRadiusX;
*/

		assert(D3DVP[0]==0 && D3DVP[1]==0);			// otherwise the following lines don't make sense

//		float fRadX = max(-fMinX/D3DVP[2]*2.0f+1.0f,fMaxX/D3DVP[2]*2.0f-1.0f);
//		float fRadY = max(-fMinY/D3DVP[3]*2.0f+1.0f,fMaxY/D3DVP[3]*2.0f-1.0f);
		m_LastCamera.wL = (fMinX/D3DVP[2]*2-1);
		m_LastCamera.wR = (fMaxX/D3DVP[2]*2-1);
		m_LastCamera.wT = (fMaxY/D3DVP[3]*2-1);
		m_LastCamera.wB = (fMinY/D3DVP[3]*2-1);

		iSystem->GetI3DEngine()->DebugDraw_PushFrustrum( "CREImposter::PrepareForUpdate", m_LastCamera, ColorB(210,210,255,16),fDistance );

    m_fRadiusX = 0.5f * (m_LastCamera.wR - m_LastCamera.wL) * fDistance / m_LastCamera.Near;
    m_fRadiusY = 0.5f * (m_LastCamera.wT - m_LastCamera.wB) * fDistance / m_LastCamera.Near;

		m_vQuadCorners[0]=vUnProjPos[0]-m_vPos;
		m_vQuadCorners[1]=vUnProjPos[1]-m_vPos;
		m_vQuadCorners[2]=vUnProjPos[2]-m_vPos;
		m_vQuadCorners[3]=vUnProjPos[3]-m_vPos;
		m_nLastBestEdge=dwBestEdge;

    m_bScreenImposter = false;	
    // store points used in later error estimation 
    m_vNearPoint   =  -m_LastCamera.Z * m_LastCamera.Near + m_LastCamera.Orig;
    m_vFarPoint    =  -m_LastCamera.Z * m_LastCamera.Far  + m_LastCamera.Orig;
  }
  else 
  { // inside cloud
    //m_LastCamera.Far = m_LastCamera.Near + 3 * fRadius;
    m_bScreenImposter =  true;
  }

	return true;
}

bool CREImposter::UpdateImposter()
{
  if (!PrepareForUpdate())
    return true;

  //PrepareForUpdate();

  PROFILE_FRAME(Imposter_Update);

  CD3D9Renderer *rd = gcpRendD3D;

  int iResX = 1 << m_nLogResolutionX;
  int iResY = 1 << m_nLogResolutionY;

  rd->EF_SetState(GS_DEPTHWRITE);

  int iOldVP[4];
  rd->GetViewport(&iOldVP[0], &iOldVP[1], &iOldVP[2], &iOldVP[3]);

  IDynTexture **pDT; //, *pDTDepth;
  if (!m_bSplit)
  {
    if (!m_bScreenImposter)
      pDT = &m_pTexture;
    else
      pDT = &m_pScreenTexture;

    if (!*pDT)
      *pDT = new SDynTexture2(iResX, iResY, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "Imposter", eTP_Clouds);

		//if (!m_pTextureDepth)
    //  m_pTextureDepth = new SDynTexture2(iResX, iResY, eTF_G16R16F, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "ImposterDepth");

    rd->m_RP.m_PS.m_NumImpostersUpdates++;

    if (*pDT) // && m_pTextureDepth)
    {
      //pDTDepth = m_pTextureDepth;
      (*pDT)->Update(iResX, iResY);
			CTexture *pT = (CTexture *)(*pDT)->GetTexture();
      int nSize = pT->GetDataSize();
      m_MemUpdated += nSize / 1024;
      rd->m_RP.m_PS.m_ImpostersSizeUpdate += nSize;
      //pDTDepth->Update(iResX, iResY);
      //ColorF col = ColorF(0,0,0,0);
      //pDT->m_pTexture->Fill(col);
      //pDTDepth->m_pTexture->Fill(Col_White);
      SD3DSurface *pDepth = rd->FX_GetDepthSurface(iResX, iResY, false);
			(*pDT)->SetRT(0, true, pDepth);
      //rd->FX_PushRenderTarget(1, pDTDepth->m_pTexture, NULL);
      gTexture = pT;
      //gTexture2 = pDTDepth->m_pTexture;
			ColorF black(0,0,0,0);
      rd->EF_ClearBuffers(FRT_CLEAR, &black);
      float fYFov, fXFov, fAspect, fNearest, fFar;
      m_LastCamera.GetPerspectiveParams(&fYFov, &fXFov, &fAspect, &fNearest, &fFar);
      CCamera EngCam;
      CCamera OldCam = rd->GetCamera();
      int nW = iResX;
      int nH = iResY;
      //fXFov = DEG2RAD(fXFov);
      fYFov = DEG2RAD(fYFov);
      fXFov = DEG2RAD(fXFov);
      if (m_bScreenImposter)
      {
        nW = rd->GetWidth();
        nH = rd->GetHeight();
        fXFov = EngCam.GetFov();
      }
      Matrix34 matr;
      matr = Matrix34::CreateFromVectors(m_LastCamera.X, -m_LastCamera.Z, m_LastCamera.Y, m_LastCamera.Orig);
      EngCam.SetMatrix(matr);
      EngCam.SetFrustum(nW, nH, fXFov, fNearest, fFar);
      //rd->SetCamera(EngCam);
      //m_LastCamera.Far += 100;
      mathMatrixTranspose(rd->m_TranspOrigCameraProjMatrix.GetData(), rd->m_CameraProjMatrix.GetData(), g_CpuFlags);
      rd->SetRCamera(m_LastCamera);

      if (rd->m_LogFile)
        rd->Logv(SRendItem::m_RecurseLevel, " +++ Start Imposter scene +++ \n");

      int nFL = rd->m_RP.m_PersFlags2;
      rd->m_RP.m_PersFlags2 |= RBPF2_IMPOSTERGEN | RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST;
      
			assert(!"GetI3DEngine()->RenderImposterContent() does not exist");
			//iSystem->GetI3DEngine()->RenderImposterContent(this, EngCam);
      rd->m_RP.m_PersFlags2 = nFL;

      if (rd->m_LogFile)
        rd->Logv(SRendItem::m_RecurseLevel, " +++ End Imposter scene +++ \n");

      if (rd->m_LogFile)
        rd->Logv(SRendItem::m_RecurseLevel, " +++ Postprocess Imposter +++ \n");

			(*pDT)->RestoreRT(0, true);
      //rd->FX_PopRenderTarget(1);

      // post-process imposter
      /*{
        SDynTexture2 *tpSrcTemp = new SDynTexture2(iResX, iResY, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempImposterRT");
        rd->m_RP.m_PersFlags &= ~(RBPF_VSNEEDSET | RBPF_PSNEEDSET);
        rd->m_RP.m_FlagsStreams = 0;
        rd->m_RP.m_FlagsPerFlush = 0;
        rd->EF_CommitShadersState();
        rd->EF_Scissor(false, 0, 0, 0, 0);
        rd->SetCullMode(R_CULL_NONE);

        // setup screen aligned quad
        struct_VERTEX_FORMAT_P3F_TEX2F pScreenBlur[] =  
        {
          Vec3(0, 0, 0), 0, 0,   
          Vec3(0, 1, 0), 0, 1,    
          Vec3(1, 0, 0), 1, 0,   
          Vec3(1, 1, 0), 1, 1,   
        };     
        rd->Set2DMode(true, 1, 1);
        CTexture::BindNULLFrom(1);
        rd->EF_SelectTMU(0);
        CTexture *tpSrc = pDT->m_pTexture;
        STexState sTexStatePoint = STexState(FILTER_POINT, true);
        STexState sTexStateLinear = STexState(FILTER_LINEAR, true);
        tpSrcTemp->SetRT(0, true, pDepth);
        rd->SetViewport(0, 0, iResX, iResY);
        tpSrc->Apply(0, &sTexStateLinear); 
        tpSrc->Apply(1, &sTexStatePoint); 

        uint nPasses = 0;
        uint nP;
        CShader *pSH = rd->m_cEF.m_ShaderCommon;
        if (pSH)
        {
          pSH->FXSetTechnique("ImposterPostprocess");
          pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

          rd->EF_SetState(GS_NODEPTHTEST);

          for (nP=0; nP<nPasses; nP++)
          {
            pSH->FXBeginPass(nP);

            float sW[9] = {0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f};

            Vec4 v;
            v[0] = 1.0f / (float)iResX;
            v[1] = 1.0f / (float)iResY;
            v[2] = 0;
            v[3] = 0;
						static CCryName PixelOffsetName("PixelOffset");
            pSH->FXSetVSFloat(PixelOffsetName, &v, 1);

            // Draw a fullscreen quad to sample the RT
            rd->DrawTriStrip(&(CVertexBuffer (pScreenBlur,VERTEX_FORMAT_P3F_TEX2F)), 4);  

            pSH->FXEndPass();
          }
          pSH->FXEnd();

        }
        delete pDT;
        m_pTexture = tpSrcTemp;
        rd->Set2DMode(false, 1, 1);
        rd->FX_PopRenderTarget(0);

        if (rd->m_LogFile)
          rd->Logv(SRendItem::m_RecurseLevel, " +++ Finish Postprocess Imposter +++ \n");
      }*/

      rd->SetCamera(OldCam);
    }
  }
  rd->SetViewport(iOldVP[0], iOldVP[1], iOldVP[2], iOldVP[3]);

  return true;
}

bool CREImposter::Display(bool bDisplayFrontOfSplit)
{
  if (SRendItem::m_RecurseLevel > 1)
    return false;

  //return true;
  CD3D9Renderer *rd = gcpRendD3D;
  CShader *pSH = rd->m_RP.m_pShader;
  SShaderTechnique *pSHT = rd->m_RP.m_pCurTechnique;
  SShaderPass *pPass = rd->m_RP.m_pCurPass;
  rd->m_RP.m_PS.m_NumImpostersDraw++;

  Vec3 vPos = m_vPos;

  uint nPasses = 0;
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  STexState sTexStatePoint = STexState(FILTER_POINT, true);
  STexState sTexStateLinear = STexState(FILTER_LINEAR, true);
  if (!m_pTexture || (bDisplayFrontOfSplit && !m_pFrontTexture))
    Warning("WRANING: CREImposter::mfDisplay: missing texture!");
  else
  {
    IDynTexture *pDT;
    if (bDisplayFrontOfSplit)
      pDT = m_pFrontTexture;
    else
      pDT = m_pTexture;
    pDT->Apply(0, CTexture::GetTexState(sTexStateLinear));
    pDT->Apply(1, CTexture::GetTexState(sTexStatePoint));

    //pDT = m_pTextureDepth;
    //pDT->Apply(1);
  }

  int State = m_State; //GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA | GS_ALPHATEST_GREATER0;
  if (m_bSplit)
  {
    if (!bDisplayFrontOfSplit)
      State |= GS_DEPTHWRITE;
    else
      State |= GS_NODEPTHTEST;
  }
  rd->EF_SelectTMU(0);
  rd->EF_SetState(State, m_AlphaRef);
  if (CRenderer::CV_r_usezpass && CTexture::m_Text_ZTarget)
    rd->FX_PushRenderTarget(1, CTexture::m_Text_ZTarget, NULL);

  Vec3 x, y, z;

  if (!m_bScreenImposter)
  {
    z  =    vPos - m_LastCamera.Orig;
    z.Normalize();
    x  =    (z ^ m_LastCamera.Y);
    x.Normalize();
    x *=    m_fRadiusX;
    y  =    (x ^ z);
    y.Normalize();
    y *=    m_fRadiusY;

    const CRenderCamera &cam = rd->GetRCamera();
    D3DXMATRIX *m = (D3DXMATRIX*)rd->m_matView->GetTop();
    cam.GetModelviewMatrix(*m);
    m = (D3DXMATRIX*)rd->m_matProj->GetTop();
    mathMatrixPerspectiveOffCenter((Matrix44*)m, cam.wL, cam.wR, cam.wB, cam.wT, cam.Near, cam.Far);

    rd->D3DSetCull(eCULL_None);
    pSH->FXBeginPass(0);

    rd->DrawQuad3D(vPos-y-x, vPos-y+x, vPos+y+x, vPos+y-x, Col_White, 0, 1, 1, 0);

    if (CRenderer::CV_r_impostersdraw & 4)
      rd->GetIRenderAuxGeom()->DrawAABB(AABB(m_WorldSpaceBV.GetMin(),m_WorldSpaceBV.GetMax()), false, Col_White, eBBD_Faceted);
    if (CRenderer::CV_r_impostersdraw & 2)
    {
      CRenderObject *pObj = rd->m_RP.m_pCurObject;
      int colR = ((DWORD)((UINT_PTR)pObj)>>4) & 0xf;
      int colG = ((DWORD)((UINT_PTR)pObj)>>8) & 0xf;
      int colB = ((DWORD)((UINT_PTR)pObj)>>12) & 0xf;
      ColorB col = Col_Yellow; //ColorB(colR<<4, colG<<4, colB<<4, 255);
      Vec3 v[4];
      v[0] = vPos-y-x; v[1] = vPos-y+x; v[2] = vPos+y+x; v[3] = vPos+y-x;
      uint16 inds[6];
      inds[0] = 0; inds[1] = 1; inds[2] = 2; inds[3] = 0; inds[4] = 2; inds[5] = 3;

      SAuxGeomRenderFlags auxFlags;
      auxFlags.SetFillMode(e_FillModeWireframe);
      auxFlags.SetDepthTestFlag(e_DepthTestOn);
      rd->GetIRenderAuxGeom()->SetRenderFlags(auxFlags);
      rd->GetIRenderAuxGeom()->DrawTriangles(v, 4, inds, 6, col);
    }
  }
  else
  {
    x  =  m_LastCamera.X;
    x *=  0.5f * (m_LastCamera.wR - m_LastCamera.wL);
    y  =  m_LastCamera.Y;
    y *=  0.5f * (m_LastCamera.wT - m_LastCamera.wB);
    z  =  -m_LastCamera.Z;
    z *=  m_LastCamera.Near;

    if (CRenderer::CV_r_impostersdraw & 4)
      rd->GetIRenderAuxGeom()->DrawAABB(AABB(m_WorldSpaceBV.GetMin(),m_WorldSpaceBV.GetMax()), false, Col_Red, eBBD_Faceted);

    // draw a polygon with this texture...
    rd->m_matProj->Push();
    mathMatrixOrthoOffCenter((Matrix44*)rd->m_matProj->GetTop(), -1, 1, -1, 1, -1, 1);

    rd->PushMatrix();
    rd->m_matView->LoadIdentity();
    pSH->FXBeginPass(0);

    rd->DrawQuad3D(Vec3(-1,-1,0), Vec3(1,-1,0), Vec3(1,1,0), Vec3(-1,1,0), Col_White, 0, 1, 1, 0);

    rd->PopMatrix();
    rd->m_matProj->Pop();
    rd->EF_DirtyMatrix();
  }

  pSH->FXEndPass();
  pSH->FXEnd();

  if (CRenderer::CV_r_usezpass && CTexture::m_Text_ZTarget)
    rd->FX_PopRenderTarget(1);

  rd->m_RP.m_pShader = pSH;
  rd->m_RP.m_pCurTechnique = pSHT;
  rd->m_RP.m_pCurPass = pPass;
  rd->m_RP.m_FrameObject++;
  rd->m_RP.m_PersFlags &= ~RBPF_WASWORLDSPACE;

  return true;
}

bool CREImposter::mfDraw(CShader *ef, SShaderPass *pPass)
{
  if (!CRenderer::CV_r_impostersdraw)
    return true;

  Display(false);

  if (IsSplit())
  {
    // display contained instances -- first opaque, then transparent.
    /*InstanceIterator ii;
    for (ii = containedOpaqueInstances.begin(); ii != containedOpaqueInstances.end(); ++ii)
    FAIL_RETURN((*ii)->Display());
    for (ii = containedTransparentInstances.begin(); ii != containedTransparentInstances.end(); ++ii)
    FAIL_RETURN((*ii)->Display());*/

    // now display the front half of the split impostor.
    Display(true);
  }  
  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////


bool CREPanoramaCluster::UpdateImposter()
{
	PROFILE_FRAME(CREPanoramaCluster::UpdateImposter);

	CD3D9Renderer *rd = gcpRendD3D;

	if(m_vPrevCamPos.GetDistance(rd->GetCamera().GetPosition())<1024)
		return true;

	int iResX = m_nTexRes;
	int iResY = m_nTexRes;

	rd->EF_SetState(GS_DEPTHWRITE);

	int iOldVP[4];
	rd->GetViewport(&iOldVP[0], &iOldVP[1], &iOldVP[2], &iOldVP[3]);

	SDynTexture *pDT, *pDT1;

	{
		pDT = m_pTexture;
    pDT1 = m_pTexture1;

		if (!m_pTexture)
			m_pTexture = new SDynTexture(iResX, iResY, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "Imposter");

		rd->m_RP.m_PS.m_NumImpostersUpdates++;

		if (m_pTexture) // && m_pTextureDepth)
		{
			pDT = m_pTexture;
			pDT1 = m_pTexture1;
			pDT->Update(iResX, iResY);
			int nSize = pDT->m_pTexture->GetDataSize();
      if (pDT1)
      {
        pDT1->Update(iResX, iResY);
        nSize += pDT1->m_pTexture->GetDataSize();
      }
//			m_MemUpdated += nSize / 1024;
			rd->m_RP.m_PS.m_ImpostersSizeUpdate += nSize;
			SD3DSurface *pDepth = rd->FX_GetDepthSurface(iResX, iResY, false);
			rd->FX_PushRenderTarget(0, pDT->m_pTexture, pDepth);
      if (pDT1)
			  rd->FX_PushRenderTarget(1, pDT1->m_pTexture, NULL);
			gTexture = pDT->m_pTexture;
			//gTexture2 = pDTDepth->m_pTexture;
			ColorF black(0,0,0,0);
			rd->EF_ClearBuffers(FRT_CLEAR, &black);
//			float fYFov, fXFov, fAspect, fNearest, fFar;
//			m_LastCamera.GetPerspectiveParams(&fYFov, &fXFov, &fAspect, &fNearest, &fFar);
			CCamera OldCam = rd->GetCamera();

			if (rd->m_LogFile)
				rd->Logv(SRendItem::m_RecurseLevel, " +++ Start Panorama scene +++ \n");

			int nFL = rd->m_RP.m_PersFlags2;
			rd->m_RP.m_PersFlags2 |= RBPF2_IMPOSTERGEN | RBPF2_NOSHADERFOG | RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST;
/*
			Vec3 vCameraDir = m_Camera.GetMatrix().GetColumn(1).GetNormalized();
			Vec3 vNear, vFar;
			vNear = m_Camera.GetPosition() + vCameraDir*m_Camera.GetNearPlane();
			vFar = m_Camera.GetPosition() + vCameraDir*m_Camera.GetFarPlane();
			Vec4 v4Nearest = Vec4(vNear, 1);
			Vec4 v4Far = Vec4(vFar, 1);
			Vec4 v4ZRange = Vec4(0,0,0,0);
			Vec4 v4Column2 = rd->m_CameraProjMatrix.GetColumn4(2);
			Vec4 v4Column3 = rd->m_CameraProjMatrix.GetColumn4(3);
			float fZ = v4Nearest.Dot(v4Column2);
			float fW = v4Nearest.Dot(v4Column3);
			v4ZRange.x = 0; //fZ / fW;
			//v4ZRange.x *= 0.999f;
			fZ = v4Far.Dot(v4Column2);
			fW = v4Far.Dot(v4Column3);
			v4ZRange.y = 1; //fZ / fW;
			v4ZRange.z = 1.0f / (v4ZRange.y - v4ZRange.x);
*/
			mathMatrixTranspose(rd->m_TranspOrigCameraProjMatrix.GetData(), rd->m_CameraProjMatrix.GetData(), g_CpuFlags);

 			int nOldTrisNum = rd->GetPolyCount();

			iSystem->GetI3DEngine()->RenderWorld(SHDF_ALLOWHDR | SHDF_SORT, m_Camera,"CREPanoramaCluster:UpdateImposter",m_nDLDFlags, 0);

			m_bTextureIsEmpty = (nOldTrisNum == rd->GetPolyCount());

			rd->m_RP.m_PersFlags2 = nFL;

			m_vPrevCamPos = m_Camera.GetPosition();
			m_vSunDir = iSystem->GetI3DEngine()->GetSunDir().normalized();
			m_vSunColor = iSystem->GetI3DEngine()->GetSunColor();
			m_vSkyColor = iSystem->GetI3DEngine()->GetSkyColor();

			if (rd->m_LogFile)
				rd->Logv(SRendItem::m_RecurseLevel, " +++ End Panorama scene +++ \n");

			if (rd->m_LogFile)
				rd->Logv(SRendItem::m_RecurseLevel, " +++ Postprocess Panorama +++ \n");

			rd->FX_PopRenderTarget(0);
      if (pDT1)
        rd->FX_PopRenderTarget(1);

			rd->SetCamera(OldCam);
		}
	}
	rd->SetViewport(iOldVP[0], iOldVP[1], iOldVP[2], iOldVP[3]);

	return true;
}

bool CREPanoramaCluster::Display(bool bDisplayFrontOfSplit)
{
	if (SRendItem::m_RecurseLevel > 1 || m_bTextureIsEmpty)
		return false;

	//return true;
	CD3D9Renderer *rd = gcpRendD3D;
	CShader *pSH = rd->m_RP.m_pShader;
	SShaderTechnique *pSHT = rd->m_RP.m_pCurTechnique;
	SShaderPass *pPass = rd->m_RP.m_pCurPass;
	rd->m_RP.m_PS.m_NumImpostersDraw++;

	pSH->FXSetTechnique("PanoramaCluster");

	uint nPasses = 0;
	pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	STexState sTexStatePoint = STexState(FILTER_POINT, true);
	STexState sTexStateLinear = STexState(FILTER_LINEAR, true);
	if (!m_pTexture)
		Warning("WARNING: CREPanoramaCluster::mfDisplay: missing texture!");
	else
	{
		SDynTexture * pDT = m_pTexture;
		pDT->Apply(0, CTexture::GetTexState(sTexStateLinear));
		pDT->Apply(1, CTexture::GetTexState(sTexStatePoint));

		pDT = m_pTexture1;
    if (pDT)
		  pDT->Apply(2, CTexture::GetTexState(sTexStatePoint));
	}

	int State = GS_DEPTHWRITE; //GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA | GS_ALPHATEST_GREATER0;
/*	if (m_bSplit)
	{
		if (!bDisplayFrontOfSplit)
			State |= GS_DEPTHWRITE;
		else
			State |= GS_NODEPTHTEST;
	}*/
	rd->EF_SelectTMU(0);
	rd->EF_SetState(State, m_AlphaRef);

  if (CRenderer::CV_r_usezpass && CTexture::m_Text_ZTarget)
  {
    rd->FX_PushRenderTarget(1, CTexture::m_Text_ZTarget, NULL);
  }

//	Vec3 x, y, z;

//	if (!m_bScreenImposter)
	{
/*		z  =    vPos - m_LastCamera.Orig;
		z.Normalize();
		x  =    (z ^ m_LastCamera.Y);
		x.Normalize();
		x *=    m_fRadiusX;
		y  =    (x ^ z);
		y.Normalize();
		y *=    m_fRadiusY;
*/

/*		Vec4 v4ZRange = CHWShader_D3D::m_CurPSParams[PSCONST_ZRANGE];
		float fFar = rd->GetCamera().GetFarPlane();
		float fNear = rd->GetCamera().GetNearPlane();
		float fA = fFar / (fFar - fNear);
		float fB = fNear * fFar / (fNear - fFar);
		v4ZRange.z = fA;
		v4ZRange.w = fB;
		float fZ = 1;
		float fLZ = (fB / (fZ - fA)); // / fFar;
*/

		rd->D3DSetCull(eCULL_None);
		pSH->FXBeginPass(0);

		// set imposter camera pos
		Vec4 v4CamPos(m_vPrevCamPos.x, m_vPrevCamPos.y, m_vPrevCamPos.z, 0);
		static CCryName ImposterCameraPosName("ImposterCameraPos");
		pSH->FXSetVSFloat(ImposterCameraPosName,&v4CamPos,1);

		Vec4 v4CamDir(m_Camera.GetMatrix().GetColumn(1).GetNormalized(), 0);
		static CCryName ImposterCameraFrontName("ImposterCameraFront");
		pSH->FXSetVSFloat(ImposterCameraFrontName,&v4CamDir,1);

		Vec4 v4CamDirUp(m_Camera.GetMatrix().GetColumn(2).GetNormalized(), 0);
		static CCryName ImposterCameraUpName("ImposterCameraUp");
		pSH->FXSetVSFloat(ImposterCameraUpName,&v4CamDirUp,1);

		Vec3 arrFrustVerts[8];
		memset(arrFrustVerts,0,sizeof(arrFrustVerts));
		m_Camera.GetFrustumVertices(arrFrustVerts);
		Vec3 vCamPos = m_Camera.GetPosition();
		for(int v=0; v<4; v++)
			arrFrustVerts[v] = vCamPos + (arrFrustVerts[v] - vCamPos).normalized()*m_Camera.GetNearPlane()*1.4f;

		rd->DrawQuad3D(
			arrFrustVerts[2], 
			arrFrustVerts[3], 
			arrFrustVerts[1], 
			arrFrustVerts[0], 
			Col_White, 0, 1, 1, 0);

	//	if (CRenderer::CV_r_impostersdraw & 4)
//			rd->GetIRenderAuxGeom()->DrawAABB(AABB(m_WorldSpaceBV.GetMin(),m_WorldSpaceBV.GetMax()), false, Col_White, eBBD_Faceted);
/*		if (CRenderer::CV_r_impostersdraw & 2)
		{
			CRenderObject *pObj = rd->m_RP.m_pCurObject;
			int colR = ((DWORD)(pObj)>>4) & 0xf;
			int colG = ((DWORD)(pObj)>>8) & 0xf;
			int colB = ((DWORD)(pObj)>>12) & 0xf;
			ColorB col = Col_Yellow; //ColorB(colR<<4, colG<<4, colB<<4, 255);
			Vec3 v[4];
			v[0] = vPos-y-x; v[1] = vPos-y+x; v[2] = vPos+y+x; v[3] = vPos+y-x;
			uint16 inds[6];
			inds[0] = 0; inds[1] = 1; inds[2] = 2; inds[3] = 0; inds[4] = 2; inds[5] = 3;

			SAuxGeomRenderFlags auxFlags;
			auxFlags.SetFillMode(e_FillModeWireframe);
			auxFlags.SetDepthTestFlag(e_DepthTestOn);
			rd->GetIRenderAuxGeom()->SetRenderFlags(auxFlags);
			rd->GetIRenderAuxGeom()->DrawTriangles(v, 4, inds, 6, col);
		}*/
	}
/*	else
	{
		x  =  m_LastCamera.X;
		x *=  0.5f * (m_LastCamera.wR - m_LastCamera.wL);
		y  =  m_LastCamera.Y;
		y *=  0.5f * (m_LastCamera.wT - m_LastCamera.wB);
		z  =  -m_LastCamera.Z;
		z *=  m_LastCamera.Near;

		if (CRenderer::CV_r_impostersdraw & 4)
			rd->GetIRenderAuxGeom()->DrawAABB(AABB(m_WorldSpaceBV.GetMin(),m_WorldSpaceBV.GetMax()), false, Col_Red, eBBD_Faceted);

		// draw a polygon with this texture...
		rd->m_matProj->Push();
		D3DXMatrixOrthoOffCenterRH(rd->m_matProj->GetTop(), -1, 1, -1, 1, -1, 1);

		rd->PushMatrix();
		rd->m_matView->LoadIdentity();
		pSH->FXBeginPass(0);

		rd->DrawQuad3D(Vec3(-1,-1,0), Vec3(1,-1,0), Vec3(1,1,0), Vec3(-1,1,0), Col_White, 0, 1, 1, 0);

		rd->PopMatrix();
		rd->m_matProj->Pop();
		rd->EF_DirtyMatrix();
	}*/

	pSH->FXEndPass();
	pSH->FXEnd();

  if (CRenderer::CV_r_usezpass && CTexture::m_Text_ZTarget)
  {
    rd->FX_PopRenderTarget(1);
  }

	rd->m_RP.m_pShader = pSH;
	rd->m_RP.m_pCurTechnique = pSHT;
	rd->m_RP.m_pCurPass = pPass;
	rd->m_RP.m_FrameObject++;
	rd->m_RP.m_PersFlags &= ~RBPF_WASWORLDSPACE;

	return true;
}

bool CREPanoramaCluster::mfDraw(CShader *ef, SShaderPass *pPass)
{
	if (!CRenderer::CV_r_impostersdraw)
		return true;

	Display(false);

	return true;
}

