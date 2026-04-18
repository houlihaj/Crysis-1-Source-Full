/*=============================================================================
  D3DRECloud.cpp : D3D specific volumetric clouds rendering.
  Copyright 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"

//=======================================================================

extern CTexture *gTexture;

void CRECloud::IlluminateCloud(Vec3 vLightPos, Vec3 vObjPos, ColorF cLightColor, ColorF cAmbColor, bool bReset)
{
  int iOldVP[4];	

  CD3D9Renderer *rd = gcpRendD3D;
  int nShadeRes = m_siShadeResolution;
  nShadeRes = 256;

  rd->GetViewport(&iOldVP[0], &iOldVP[1], &iOldVP[2], &iOldVP[3]);

  rd->EF_PushMatrix();
  rd->m_matProj->Push();
  rd->SetViewport(0, 0, nShadeRes, nShadeRes);

  Vec3 vDir = vLightPos;
  vDir.Normalize();

  if (bReset)
    m_lightDirections.clear();
  m_lightDirections.push_back(vDir);

  vLightPos *= (1.1f*m_boundingBox.GetRadius());
  vLightPos += m_boundingBox.GetCenter();

  CRenderCamera cam;

  Vec3 vUp(0, 0, 1);
  cam.LookAt(vLightPos, m_boundingBox.GetCenter(), vUp);

  SortParticles(cam.ViewDir(), vLightPos, eSort_AWAY);

  float DistToCntr = (m_boundingBox.GetCenter()-vLightPos)*cam.ViewDir();

  float fNearDist = DistToCntr - m_boundingBox.GetRadius();
  float fFarDist  = DistToCntr + m_boundingBox.GetRadius();

  D3DXMATRIX *m = (D3DXMATRIX*)rd->m_matView->GetTop();
  cam.GetModelviewMatrix(*m);

  mathMatrixOrthoOffCenter((Matrix44*)rd->m_matProj->GetTop(), -m_boundingBox.GetRadius(), m_boundingBox.GetRadius(), -m_boundingBox.GetRadius(), m_boundingBox.GetRadius(), fNearDist, fFarDist);
  rd->EF_DirtyMatrix();

  rd->SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
  rd->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST | GS_ALPHATEST_GREATER, 0);
  rd->D3DSetCull(eCULL_None);

	CRenderObject *pObj = rd->m_RP.m_pCurObject;
	CShader *pSH = rd->m_RP.m_pShader;
	SShaderTechnique *pSHT = rd->m_RP.m_pCurTechnique;
	SShaderPass *pPass = rd->m_RP.m_pCurPass;
	SRenderShaderResources *pShRes = rd->m_RP.m_pShaderResources;

	if (pShRes && pShRes->m_Textures[EFTT_DIFFUSE])
	{
		m_pTexParticle = pShRes->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex;
	}

  m_pTexParticle->Apply(0);

  
  rd->FX_SetFPMode();
  HRESULT h = S_OK;

#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3DSURFACE9 pTargetSurf = NULL;
  h = rd->m_pd3dDevice->CreateRenderTarget(nShadeRes, nShadeRes, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pTargetSurf, NULL);
  if (FAILED(h))
    return;

  SD3DSurface *pDepth = rd->FX_GetDepthSurface(nShadeRes, nShadeRes, false);
  rd->FX_PushRenderTarget(0, pTargetSurf, pDepth);
#elif defined (DIRECT3D10)
  assert(0);
#endif
	ColorF white(Col_White);
  rd->EF_ClearBuffers(FRT_CLEAR, &white);
  rd->EF_Commit();

  float fPixelsPerLength = (float)nShadeRes / (2.0f * m_boundingBox.GetRadius());

  // the solid angle over which we will sample forward-scattered light.
  float fSolidAngle = 0.09f;
  int i;
  int iNumFailed = 0;
  int nParts = m_particles.size();
  for (i=0; i<nParts; i++)
  {
    SCloudParticle *p = m_particles[i];
    Vec3 vParticlePos = p->GetPosition();

    Vec3 vOffset = vLightPos - vParticlePos;

    float fDistance  = fabs(cam.ViewDir() * vOffset) - fNearDist;

    float fArea      = fSolidAngle * fDistance * fDistance;
    int   iPixelDim  = (int)(sqrtf(fArea) * fPixelsPerLength);
    //iPixelDim = 1;
    int   iNumPixels = iPixelDim * iPixelDim;
    if (iNumPixels < 1) 
    {
      iNumPixels = 1;
      iPixelDim = 1;
    }

    // the scale factor to convert the read back pixel colors to an average illumination of the area.
    float fColorScaleFactor = fSolidAngle / (iNumPixels * 255.0f);

    unsigned char *ds = new unsigned char[4 * iNumPixels];
    unsigned char *c = ds;

    Vec3 vWinPos;

    // find the position in the buffer to which the particle position projects.
    rd->ProjectToScreen(vParticlePos.x, vParticlePos.y, vParticlePos.z,  &vWinPos.x, &vWinPos.y, &vWinPos.z);
    vWinPos.x /= 100.0f/rd->m_NewViewport.Width;
    vWinPos.y /= 100.0f/rd->m_NewViewport.Height;

    // offset the projected window position by half the size of the readback region.
    vWinPos.x -= 0.5f * iPixelDim;
    if (vWinPos.x < 0)
      vWinPos.x = 0;
    vWinPos.y = (vWinPos.y - 0.5f * iPixelDim);
    if (vWinPos.y < 0)
      vWinPos.y = 0;

    // read back illumination of this particle from the buffer.
    // Copy data
    D3DLOCKED_RECT d3dlrTarg;
    memset(&d3dlrTarg, 0, sizeof(d3dlrTarg));
#if defined (DIRECT3D9) || defined (OPENGL)
    h = pTargetSurf->LockRect(&d3dlrTarg, NULL, 0);
#elif defined (DIRECT3D10)
    assert(0);
#endif
    if (!FAILED(h))
    {
      byte *ss = (byte *)d3dlrTarg.pBits;
      ss += (int)vWinPos.x*4 + (int)vWinPos.y*d3dlrTarg.Pitch;
      for (int h=0; h<iPixelDim; h++)
      {
        memcpy(ds, ss, iPixelDim*4);
        ds += iPixelDim*4;
        ss += d3dlrTarg.Pitch;
      }
    }
    char name[128];
    static int num;
    sprintf(name, "scat_%3d.jpg", num);
    num++;
    //::WriteTGA((byte *)d3dlrTarg.pBits, nShadeRes, nShadeRes, name, 32);
    //::WriteJPG((byte *)d3dlrTarg.pBits, nShadeRes, nShadeRes, name, 32);
#if defined (DIRECT3D9) || defined (OPENGL)
    h = pTargetSurf->UnlockRect();
#elif defined (DIRECT3D10)
    assert(0);
#endif

    // scattering coefficient vector.
    //m_sfScatterFactor = m_sfAlbedo * 80 * (1.0f/(4.0f*(float)M_PI));
    ColorF vScatter = ColorF(m_sfScatterFactor, m_sfScatterFactor, m_sfScatterFactor, 1);

    // add up the read back pixels (only need one component -- its grayscale)
    int iSum = 0;
    int nTest = 0;
    for (int k = 0; k < 4 * iNumPixels; k+=4)
    {
      iSum += c[k];
      nTest += 255;
    }
    delete [] c;

    ColorF vScatteredAmount = ColorF(iSum * fColorScaleFactor, iSum * fColorScaleFactor, iSum * fColorScaleFactor, 1 - m_sfTransparency);
    vScatteredAmount *= vScatter;

    ColorF vColor = vScatteredAmount;
    float fScat = (float)iSum / (float)nTest;
    fScat = powf(fScat, 3);
    //vColor = ColorF(fScat);

    vColor      *= cLightColor;  
    vColor.a     = 1 - m_sfTransparency;

    //ColorF Amb = ColorF(0.1f, 0.1f, 0.1f, 1.0f);
    if (bReset)
    {
      p->SetBaseColor(cAmbColor);  
      p->ClearLitColors();
      p->AddLitColor(vColor);
    }
    else
    {
      p->AddLitColor(vColor);
    }

    vScatteredAmount *= 1.5f;

    // clamp the color
    vScatteredAmount.clamp();
    vScatteredAmount.a = 1 - m_sfTransparency;

    Vec3 vPos = vParticlePos;
    Vec3 x = cam.X * p->GetRadiusX();
    Vec3 y = cam.Y * p->GetRadiusY();
    rd->DrawQuad3D(vPos-y-x, vPos-y+x, vPos+y+x, vPos+y-x, vScatteredAmount, p->m_vUV[0].x, p->m_vUV[0].y, p->m_vUV[1].x, p->m_vUV[1].y);
  }

  rd->FX_PopRenderTarget(0);
#if defined (DIRECT3D9) || defined (OPENGL)
  SAFE_RELEASE (pTargetSurf);
#elif defined (DIRECT3D10)
  assert(0);
#endif
  rd->m_RP.m_pCurObject = pObj;
  rd->m_RP.m_pCurInstanceInfo = &rd->m_RP.m_pCurObject->m_II;
  rd->m_RP.m_pShader = pSH;
  rd->m_RP.m_pCurTechnique = pSHT;
  rd->m_RP.m_pCurPass = pPass;

  rd->PopMatrix();
  rd->m_matProj->Pop();
  rd->EF_DirtyMatrix();
  rd->SetViewport(iOldVP[0], iOldVP[1], iOldVP[2], iOldVP[3]);
}

void CRECloud::DisplayWithoutImpostor(const CRenderCamera& camera)
{
  CD3D9Renderer *rd = gcpRendD3D;

  // copy the current camera
  CRenderCamera cam(camera);

	Vec3 vUp(0, 0, 1);

	Vec3 vParticlePlane = cam.X % cam.Y;
	Vec3 vParticleX = (vUp % vParticlePlane).GetNormalized();
	Vec3 vParticleY = (vParticleX % vParticlePlane).GetNormalized();

	// texture is rotated to get minimum texture occupation so we need compensate that
//	Matrix33 mCamInv = Matrix33(cam.X.GetNormalized(),cam.Y.GetNormalized(),vParticlePlane.GetNormalized());		mCamInv.Invert();

//	Vec3 vParticleX = mCamInv.TransformVector(vParticleWSX);
//	Vec3 vParticleY = mCamInv.TransformVector(vParticleWSY);

//	Vec3 vParticleX = cam.X;
//	Vec3 vParticleY = cam.Y;

	float fCosAngleSinceLastSort = m_vLastSortViewDir * rd->GetRCamera().ViewDir();
  Vec3 vSkyColor = iSystem->GetI3DEngine()->GetSkyColor();
  ColorF cSkyColor = ColorF(vSkyColor.x, vSkyColor.y, vSkyColor.z, 1);

  float fSquareDistanceSinceLastSort = (rd->GetRCamera().Orig - m_vLastSortCamPos).GetLengthSquared();

  if (fCosAngleSinceLastSort < m_sfSortAngleErrorTolerance || fSquareDistanceSinceLastSort > m_sfSortSquareDistanceTolerance)
  {
    Vec3 vSortPos = -cam.ViewDir();
    vSortPos *= (1.1f * m_boundingBox.GetRadius());

    // sort the particles from back to front wrt the camera position.
    SortParticles(cam.ViewDir(), vSortPos, eSort_TOWARD);

    m_vLastSortViewDir = rd->GetRCamera().ViewDir();
    m_vLastSortCamPos = rd->GetRCamera().Orig;
  } 

  ColorF color;
  Vec3 eyeDir;

  int nParts = m_particles.size();
  int nStartPart = 0;
  int nCurParts;
  
	CRenderObject *pObj = rd->m_RP.m_pCurObject;
	CShader *pSH = rd->m_RP.m_pShader;
	SShaderTechnique *pSHT = rd->m_RP.m_pCurTechnique;
	SShaderPass *pPass = rd->m_RP.m_pCurPass;
	SRenderShaderResources *pShRes = rd->m_RP.m_pShaderResources;
  CREImposter *pRE = (CREImposter *)pObj->m_pRE;
  Vec3 vPos = pRE->GetPosition();

  uint nPasses;
  if (SRendItem::m_RecurseLevel > 1)
	{
		static CCryName techName("Cloud_Recursive");
    pSH->FXSetTechnique(techName);
	}
  else
	{
		static CCryName techName("Cloud");
    pSH->FXSetTechnique(techName);
	}
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	if (pShRes && pShRes->m_Textures[EFTT_DIFFUSE])
	{
		m_pTexParticle = pShRes->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex;
	}
  pSH->FXBeginPass(0);
  m_pTexParticle->Apply(0);
/*
	// set depth texture for soft clipping of cloud particle against scene geometry
	if(0 != CTexture::m_Text_ZTarget)
	{
		STexState depthTextState( FILTER_POINT, true );
		CTexture::m_Text_ZTarget->Apply( 1, CTexture::GetTexState(depthTextState) );
	}
*/
  rd->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST | GS_ALPHATEST_GREATER, 0);
  //rd->EF_SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST | GS_ALPHATEST_GREATER, 0);
  rd->D3DSetCull(eCULL_None);

  if (SRendItem::m_RecurseLevel > 1)
  {
    rd->m_cEF.m_RTRect = Vec4(0,0,1,1);
  }

  if (SRendItem::m_RecurseLevel > 1)
  {
    Vec4 vCloudColorScale(m_fCloudColorScale,0,0,0);
		static CCryName g_CloudColorScaleName("g_CloudColorScale");
    pSH->FXSetPSFloat(g_CloudColorScaleName, &vCloudColorScale , 1);
  }

  rd->EF_Commit();

  if (!FAILED(rd->EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
    void *pVB = rd->m_pVB[0];
    HRESULT h = rd->FX_SetVStream(0, pVB, 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
    h = rd->FX_SetIStream(rd->m_pIB);
    while (nStartPart < nParts)
    {
      nCurParts = nParts - nStartPart;
      if (nCurParts > MAX_DYNVB3D_VERTS/4)
        nCurParts = MAX_DYNVB3D_VERTS/4;
      int nOffs, nIOffs;
      struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pDst = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)rd->GetVBPtr(nCurParts*4, nOffs);
      assert(pDst);
      if (!pDst)
        return;
      ushort *pDstInds = rd->GetIBPtr(nCurParts*6, nIOffs);
      assert(pDstInds);
      if (!pDstInds)
        return;
      if (pVB != rd->m_pVB[0])
      {
        pVB = rd->m_pVB[0];
        h = rd->FX_SetVStream(0, pVB, 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      }

		  // get various run-time parameters to determine cloud shading
		  Vec3 sunDir( gEnv->p3DEngine->GetSunDir().GetNormalized() );
  	
		  float minHeight( m_boundingBox.GetMin().z );
		  float totalHeight( m_boundingBox.GetMax().z - minHeight );

		  ColorF cloudSpec, cloudDiff;
		  GetIllumParams( cloudSpec, cloudDiff );

		  I3DEngine* p3DEngine( gEnv->p3DEngine );
		  assert( 0 != p3DEngine );

		  float sunLightMultiplier;
		  float skyLightMultiplier;
		  p3DEngine->GetCloudShadingMultiplier( sunLightMultiplier, skyLightMultiplier );

		  Vec3 brightColor( sunLightMultiplier * p3DEngine->GetSunColor().CompMul( Vec3( cloudSpec.r, cloudSpec.g, cloudSpec.b ) ) );
		  Vec3 darkColor( skyLightMultiplier * p3DEngine->GetSkyColor().CompMul( Vec3( cloudDiff.r, cloudDiff.g, cloudDiff.b ) ) );

		  Vec3 negCamFrontDir( -cam.ViewDir() );

		  m_fCloudColorScale=1.0f;

		  // compute m_fCloudColorScale for HDR rendering
		  {
			  if(brightColor.x>m_fCloudColorScale) m_fCloudColorScale=brightColor.x;
			  if(brightColor.y>m_fCloudColorScale) m_fCloudColorScale=brightColor.y;
			  if(brightColor.z>m_fCloudColorScale) m_fCloudColorScale=brightColor.z;

			  if(darkColor.x>m_fCloudColorScale) m_fCloudColorScale=darkColor.x;
			  if(darkColor.y>m_fCloudColorScale) m_fCloudColorScale=darkColor.y;
			  if(darkColor.z>m_fCloudColorScale) m_fCloudColorScale=darkColor.z;

			  // normalize color
			  brightColor /= m_fCloudColorScale;
			  darkColor /= m_fCloudColorScale;
		  }

		  // render cloud particles
      for ( int i=0; i<nCurParts; i++)
      {
        SCloudParticle *p = m_particles[i+nStartPart];

      //  // Start with ambient light
      //  color = p->GetBaseColor();
      //  //color = Col_White;

        //if (m_bUseAnisoLighting) // use the anisotropic scattering.
        //{
        //  eyeDir = cam.Orig;
        //  eyeDir -= (p->GetPosition() + vPos);
        //  eyeDir.Normalize();
        //  float pf;

        //  for ( int j=0; j<p->GetNumLitColors(); j++)
        //  {
        //    float fCosAlpha = m_lightDirections[j] * eyeDir;
        //    pf = 0.75f * (1.0f + fCosAlpha * fCosAlpha); // rayleigh scattering = (3/4) * (1+cos^2(alpha))
        //    const ColorF col = p->GetLitColor(j);
        //    //pf = min(pf, 1.0f);
        //    //color = LERP(cSkyColor, col, pf);
        //    color.r += col.r * pf;
        //    color.g += col.g * pf;
        //    color.b += col.b * pf;
        //  }
        //}
        //else  // just use isotropic scattering instead.
        //{      
        //  for ( int j=0; j<p->GetNumLitColors(); ++j)
        //  {
        //    color += p->GetLitColor(j);
        //  }
        //}

        // Set the transparency independently of the colors
        //color.a = 1 - m_sfTransparency;
        //if (color.r > 1.0f)
        //  color.r = 1.0f;
        //if (color.g > 1.0f)
        //  color.g = 1.0f;
        //if (color.b > 1.0f)
        //  color.b = 1.0f;
        //color *= pRE->m_fCurTransparency;

        // draw the particle as a textured billboard.
        int nInd = i*4;
        struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pQuad = &pDst[nInd];
        Vec3 pos = p->GetPosition() * m_fScale + vPos;
        Vec3 x = vParticleX * p->GetRadiusX() * m_fScale;
        Vec3 y = vParticleY * p->GetRadiusY() * m_fScale;
        //ColorB cb = color;
        //uint32 cd = cb.pack_argb8888();

			  // determine shade for each vertex of the billboard
			  float f0 = sunDir.Dot( Vec3( -y - x ).GetNormalized() ) * 0.5f + 0.5f;
			  float f1 = sunDir.Dot( Vec3( -y + x ).GetNormalized() ) * 0.5f + 0.5f;
			  float f2 = sunDir.Dot( Vec3(  y + x ).GetNormalized() ) * 0.5f + 0.5f;
			  float f3 = sunDir.Dot( Vec3(  y - x ).GetNormalized() ) * 0.5f + 0.5f;

			  Vec3 eye0( cam.Orig - ( pos - y - x ) ); eye0 = ( eye0.GetLengthSquared() < 1e-4f ) ? negCamFrontDir : eye0.GetNormalized();
			  Vec3 eye1( cam.Orig - ( pos - y + x ) ); eye1 = ( eye1.GetLengthSquared() < 1e-4f ) ? negCamFrontDir : eye1.GetNormalized();
			  Vec3 eye2( cam.Orig - ( pos + y + x ) ); eye2 = ( eye2.GetLengthSquared() < 1e-4f ) ? negCamFrontDir : eye2.GetNormalized();
			  Vec3 eye3( cam.Orig - ( pos + y - x ) ); eye3 = ( eye3.GetLengthSquared() < 1e-4f ) ? negCamFrontDir : eye3.GetNormalized();
			  f0 *= sunDir.Dot( eye0 ) * 0.25f + 0.75f;
			  f1 *= sunDir.Dot( eye1 ) * 0.25f + 0.75f;
			  f2 *= sunDir.Dot( eye2 ) * 0.25f + 0.75f;
			  f3 *= sunDir.Dot( eye3 ) * 0.25f + 0.75f;

			  //float heightScaleTop( ( p->GetPosition().z + x.z + y.z - minHeight ) / totalHeight );
			  //float heightScaleBottom( ( p->GetPosition().z - x.z - y.z - minHeight ) / totalHeight );
			  float heightScaleTop( 1 );
			  float heightScaleBottom( 1 );

			  // compute finale shading values
			  f0 = clamp_tpl( f0 * heightScaleBottom, 0.0f, 1.0f );
			  f1 = clamp_tpl( f1 * heightScaleBottom, 0.0f, 1.0f );
			  f2 = clamp_tpl( f2 * heightScaleTop, 0.0f, 1.0f );
			  f3 = clamp_tpl( f3 * heightScaleTop, 0.0f, 1.0f );

			  // blend between dark and bright cloud color based on shading value
			  Vec3 c0( darkColor + f0 * ( brightColor - darkColor ) );
			  Vec3 c1( darkColor + f1 * ( brightColor - darkColor ) );
			  Vec3 c2( darkColor + f2 * ( brightColor - darkColor ) );
			  Vec3 c3( darkColor + f3 * ( brightColor - darkColor ) );
			  float transp( pRE->m_fCurTransparency );

			  // write billboard vertices
			  ColorF col0( c0.x, c0.y, c0.z, transp ); col0.clamp();
        pQuad[0].xyz = pos - y - x;
			  pQuad[0].color.dcolor = ColorB( col0 ).pack_argb8888();
        pQuad[0].st[0] = p->m_vUV[0].x;
        pQuad[0].st[1] = p->m_vUV[0].y;

			  ColorF col1( c1.x, c1.y, c1.z, transp ); col1.clamp();
        pQuad[1].xyz = pos - y + x;
			  pQuad[1].color.dcolor = ColorB( col1 ).pack_argb8888();
        pQuad[1].st[0] = p->m_vUV[1].x;
        pQuad[1].st[1] = p->m_vUV[0].y;

			  ColorF col2( c2.x, c2.y, c2.z, transp ); col2.clamp();
        pQuad[2].xyz = pos + y + x;
			  pQuad[2].color.dcolor = ColorB( col2 ).pack_argb8888();
        pQuad[2].st[0] = p->m_vUV[1].x;
        pQuad[2].st[1] = p->m_vUV[1].y;

			  ColorF col3( c3.x, c3.y, c3.z, transp ); col3.clamp();
        pQuad[3].xyz = pos + y - x;
			  pQuad[3].color.dcolor = ColorB( col3 ).pack_argb8888();
        pQuad[3].st[0] = p->m_vUV[0].x;
        pQuad[3].st[1] = p->m_vUV[1].y;

        ushort *pInds = &pDstInds[i*6];
        pInds[0] = nInd;
        pInds[1] = nInd+1;
        pInds[2] = nInd+2;

        pInds[3] = nInd;
        pInds[4] = nInd+2;
        pInds[5] = nInd+3;
      }
      rd->UnlockVB(0);
      rd->UnlockIB();

  #if defined (DIRECT3D9) || defined (OPENGL)
      h = rd->m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, nOffs, 0, nCurParts*4, nIOffs, nCurParts*2);
  #elif defined (DIRECT3D10)
      rd->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      rd->m_pd3dDevice->DrawIndexed(nCurParts*6, nIOffs, nOffs);
  #endif
      assert(h == S_OK);

      nStartPart += nCurParts;
    }
  }

  rd->m_RP.m_pCurObject = pObj;
  rd->m_RP.m_pCurInstanceInfo = &rd->m_RP.m_pCurObject->m_II;
  rd->m_RP.m_pShader = pSH;
  rd->m_RP.m_pCurTechnique = pSHT;
  rd->m_RP.m_pCurPass = pPass;
  rd->m_RP.m_PrevLMask = -1;
}

bool CRECloud::UpdateImposter(CRenderObject *pObj)
{
  CREImposter *pRE = (CREImposter *)pObj->m_pRE;
  CD3D9Renderer *rd = gcpRendD3D;

  if (!pRE->PrepareForUpdate())
  {
    if (!CRenderer::CV_r_cloudsupdatealways && pRE->m_nFrameReset == rd->m_nFrameReset)
      return true;
  }

  PROFILE_FRAME(Imposter_CloudUpdate);

  pRE->m_nFrameReset = rd->m_nFrameReset;
  int nLogX = pRE->m_nLogResolutionX;
  int nLogY = pRE->m_nLogResolutionY;
  int iResX = 1 << nLogX;
  int iResY = 1 << nLogY;
  while (iResX > CRenderer::CV_r_texatlassize)
  {
    nLogX--;
    iResX = 1 << nLogX;
  }
  while (iResY > CRenderer::CV_r_texatlassize)
  {
    nLogY--;
    iResY = 1 << nLogY;
  }

  int iOldVP[4];
  rd->GetViewport(&iOldVP[0], &iOldVP[1], &iOldVP[2], &iOldVP[3]);

  rd->EF_PushMatrix();
  rd->m_matProj->Push();
  rd->SetViewport(0, 0, iResX, iResY);
  rd->m_RP.m_PS.m_NumCloudImpostersUpdates++;

  D3DXMATRIX *m = (D3DXMATRIX*)rd->m_matView->GetTop();
  pRE->m_LastCamera.GetModelviewMatrix(*m);
  m = (D3DXMATRIX*)rd->m_matProj->GetTop();
  //pRE->m_LastCamera.GetProjectionMatrix(*m);
  mathMatrixPerspectiveOffCenter((Matrix44*)m, pRE->m_LastCamera.wL, pRE->m_LastCamera.wR, pRE->m_LastCamera.wB, pRE->m_LastCamera.wT, pRE->m_LastCamera.Near, pRE->m_LastCamera.Far);

  IDynTexture **pDT;
  if (!pRE->m_bSplit)
  {
    if (!pRE->m_bScreenImposter)
      pDT = &pRE->m_pTexture;
    else
      pDT = &pRE->m_pScreenTexture;
    if (!*pDT)
       *pDT = new SDynTexture2(iResX, iResY, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "CloudImposter", eTP_Clouds);

    if (*pDT)
    {
      SD3DSurface *pDepth = &gcpRendD3D->m_DepthBufferOrig;
      uint32 nX1, nY1, nW1, nH1;
      (*pDT)->Update(iResX, iResY);
      (*pDT)->GetImageRect(nX1, nY1, nW1, nH1);
      if (nW1 > (int)rd->m_d3dsdBackBuffer.Width || nH1 > (int)rd->m_d3dsdBackBuffer.Height)
        pDepth = rd->FX_GetDepthSurface(nW1, nH1, false);
      (*pDT)->SetRT(0, true, pDepth);
      gTexture = (CTexture *)(*pDT)->GetTexture();

      uint32 nX, nY, nW, nH;
      (*pDT)->GetSubImageRect(nX, nY, nW, nH);
      if (pRE->m_bScreenImposter)
      {
        if (CRenderer::CV_r_cloudsdebug != 2)
          gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Generating screen '%s' - %s (%d, %d, %d, %d) (%d)\n", gTexture->GetName(), (*pDT)->IsSecondFrame() ? "Second" : "First", nX, nY, nW, nH, gRenDev->GetFrameID(false));
      }
      else
      {
        if (CRenderer::CV_r_cloudsdebug != 1)
          gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Generating '%s' - %s (%d, %d, %d, %d) (%d)\n", gTexture->GetName(), (*pDT)->IsSecondFrame() ? "Second" : "First", nX, nY, nW, nH, gRenDev->GetFrameID(false));
      }

      int nSize = iResX * iResY * 4;
      pRE->m_MemUpdated += nSize / 1024;
      rd->m_RP.m_PS.m_CloudImpostersSizeUpdate += nSize;
			ColorF black(0,0,0,0);
      rd->EF_ClearBuffers(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &black);
      DisplayWithoutImpostor(pRE->m_LastCamera);
      (*pDT)->SetUpdateMask();
      (*pDT)->RestoreRT(0, true);
    }
  }
  rd->SetViewport(iOldVP[0], iOldVP[1], iOldVP[2], iOldVP[3]);
  rd->EF_PopMatrix();
  rd->m_matProj->Pop();

  return true;
}

bool CRECloud::mfDisplay(bool bDisplayFrontOfSplit)
{
  CD3D9Renderer *rd = gcpRendD3D;
  CRenderObject *pObj = rd->m_RP.m_pCurObject;
  CREImposter *pRE = (CREImposter *)pObj->m_pRE;
  Vec3 vPos = pRE->m_vPos;
  CShader *pSH = rd->m_RP.m_pShader;
  SShaderTechnique *pSHT = rd->m_RP.m_pCurTechnique;
  SShaderPass *pPass = rd->m_RP.m_pCurPass;
	rd->m_RP.m_FrameObject++;
  rd->m_RP.m_PS.m_NumCloudImpostersDraw++;

  if (CRenderer::CV_r_cloudsdebug == 2 && pRE->m_bScreenImposter)
    return true;
  if (CRenderer::CV_r_cloudsdebug == 1 && !pRE->m_bScreenImposter)
    return true;

  uint nPasses;

  float fAlpha = pObj->m_fAlpha;
  ColorF col(1, 1, 1, fAlpha);

  if (SRendItem::m_RecurseLevel > 1)
  {
    DisplayWithoutImpostor(rd->GetRCamera());
    return true;
  }

//  if (!pRE->m_pTexture || (bDisplayFrontOfSplit && !pRE->m_pFrontTexture))
//    Warning("WARNING: CRECloud::mfDisplay: missing texture!");
//  else
//  {
//    IDynTexture *pDT;
//    if (bDisplayFrontOfSplit)
//      pDT = pRE->m_pFrontTexture;
//    else
//      pDT = pRE->m_pTexture;
    //CTexture::m_Text_NoTexture->Apply(0);
//    pDT->Apply(0);
//  }

  IDynTexture *pDT;
  if (!pRE->m_bScreenImposter)
    pDT = pRE->m_pTexture;
  else
    pDT = pRE->m_pScreenTexture;

  //float fOffsetU = 0, fOffsetV = 0;
  if (pDT && (!bDisplayFrontOfSplit || (bDisplayFrontOfSplit && pRE->m_pFrontTexture)))
  {
    pDT->Apply(0);
    
    //fOffsetU = 0.5f / (float) pDT->GetWidth();
    //fOffsetV = 0.5f / (float) pDT->GetHeight();
  }

	// set depth texture for soft clipping of cloud against scene geometry
	if(0 != CTexture::m_Text_ZTarget)
	{
		STexState depthTextState( FILTER_POINT, true );
		CTexture::m_Text_ZTarget->Apply( 1, CTexture::GetTexState(depthTextState) );
	}

  int State = pRE->m_State; //GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA | GS_ALPHATEST_GREATER0;    

  if (pRE->m_bSplit)
  {
    if (!bDisplayFrontOfSplit)
      State |= GS_DEPTHWRITE;
    else
      State |= GS_NODEPTHTEST;
  }
  

	// Martin test - for clouds particle soft clipping against terrain 
//	State |= GS_NODEPTHTEST;

  // Added for Sun-rays to work with clouds:
  // - force depth writing so that sun-rays interact with clouds. 
  // - also make sure we don't make any depth test (it's already done in pixel shader) so that no alpha test artifacts are visible 
  //State |= GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL;

  rd->EF_SelectTMU(0);
  rd->EF_SetState(State, pRE->m_AlphaRef);

  Vec3 x, y, z;

	Vec4 vCloudColorScale(m_fCloudColorScale,0,0,0);

  if (!pRE->m_bScreenImposter)
  {
    z  =    vPos - pRE->m_LastCamera.Orig;
    z.Normalize();
    x  =    (z ^ pRE->m_LastCamera.Y);
    x.Normalize();
		x *=    pRE->m_fRadiusX;
    y  =    (x ^ z);
    y.Normalize();
    y *=    pRE->m_fRadiusY;



    const CRenderCamera &cam = rd->GetRCamera();
    D3DXMATRIX *m = (D3DXMATRIX*)rd->m_matView->GetTop();
    cam.GetModelviewMatrix(*m);
    m = (D3DXMATRIX*)rd->m_matProj->GetTop();
    mathMatrixPerspectiveOffCenter((Matrix44*)m, cam.wL, cam.wR, cam.wB, cam.wT, cam.Near, cam.Far);

		if (SRendItem::m_RecurseLevel <= 1)
		{
			const CD3D9Renderer::SRenderTileInfo& rti = rd->GetRenderTileInfo();
			if (rti.nGridSizeX > 1.f || rti.nGridSizeY>1.f)
			{ // shift and scale viewport
				(*m)._11 *= rti.nGridSizeX;
				(*m)._22 *= rti.nGridSizeY;
				(*m)._31 =   (rti.nGridSizeX-1.f)-rti.nPosX*2.0f;
				(*m)._32 = -((rti.nGridSizeY-1.f)-rti.nPosY*2.0f);
			}
		}

    rd->D3DSetCull(eCULL_None);
		static CCryName techName("Cloud_Imposter");
    pSH->FXSetTechnique(techName);
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pSH->FXBeginPass(0);

		static CCryName g_CloudColorScaleName("g_CloudColorScale");
		pSH->FXSetPSFloat( g_CloudColorScaleName, &vCloudColorScale , 1);

		Vec3 lPos;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_POS, lPos );		
		Vec4 lightningPosition( lPos.x, lPos.y, lPos.z, 0.0f );		
		static CCryName LightningPosName("LightningPos");
		pSH->FXSetVSFloat( LightningPosName, &lightningPosition, 1 );

		Vec3 lCol;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_COLOR, lCol );
		Vec3 lSize;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_SIZE, lSize );
		Vec4 lightningColorSize( lCol.x, lCol.y, lCol.z, lSize.x * 0.01f );
		static CCryName LightningColSizeName("LightningColSize");
		pSH->FXSetVSFloat( LightningColSizeName, &lightningColorSize, 1 );   

		rd->DrawQuad3D( pRE->m_vQuadCorners[0]+vPos,
										pRE->m_vQuadCorners[1]+vPos,
										pRE->m_vQuadCorners[2]+vPos,
										pRE->m_vQuadCorners[3]+vPos, col, 0, 1, 1, 0); 

    if (CRenderer::CV_r_impostersdraw & 4)
    {
      rd->EF_SetState(GS_NODEPTHTEST);
      SAuxGeomRenderFlags auxFlags;
      auxFlags.SetDepthTestFlag(e_DepthTestOff);
      rd->GetIRenderAuxGeom()->SetRenderFlags(auxFlags);
      rd->GetIRenderAuxGeom()->DrawAABB(AABB(pRE->m_WorldSpaceBV.GetMin(),pRE->m_WorldSpaceBV.GetMax()), false, Col_White, eBBD_Faceted);
    }
    if (CRenderer::CV_r_impostersdraw & 2)
    {
      CRenderObject *pObj = rd->m_RP.m_pCurObject;
      int colR = ((DWORD)((UINT_PTR)pObj)>>4) & 0xf;
      int colG = ((DWORD)((UINT_PTR)pObj)>>8) & 0xf;
      int colB = ((DWORD)((UINT_PTR)pObj)>>12) & 0xf;
      ColorB col = Col_Green; //ColorB(colR<<4, colG<<4, colB<<4, 255);
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
    x  =  pRE->m_LastCamera.X;
    x *=  0.5f * (pRE->m_LastCamera.wR - pRE->m_LastCamera.wL);
    y  =  pRE->m_LastCamera.Y;
    y *=  0.5f * (pRE->m_LastCamera.wT - pRE->m_LastCamera.wB);
    z  =  -pRE->m_LastCamera.Z;
    z *=  pRE->m_LastCamera.Near;

    if (CRenderer::CV_r_impostersdraw & 4)
      rd->GetIRenderAuxGeom()->DrawAABB(AABB(pRE->m_WorldSpaceBV.GetMin(),pRE->m_WorldSpaceBV.GetMax()), false, Col_Red, eBBD_Faceted);

		//D3DXMATRIX matVP;
		//D3DXMatrixMultiply( &matVP, rd->m_matView->GetTop(), rd->m_matProj->GetTop() );

		//const Vec3& center( pRE->GetPosition() );
		//D3DXVECTOR4 centerClipSpace;
		//D3DXVec3Transform( &centerClipSpace, &D3DXVECTOR3( center.x, center.y, center.z ), &matVP );

		//float avgDepth( 0 );
		//if( centerClipSpace.w > 0 )
		//	avgDepth = centerClipSpace.z / centerClipSpace.w;

		// draw a polygon with this texture...
    rd->m_matProj->Push();
		D3DXMATRIX *m = (D3DXMATRIX*)rd->m_matProj->GetTop();
    mathMatrixOrthoOffCenterLH((Matrix44*)m, -1, 1, -1, 1, -1, 1);
		if (SRendItem::m_RecurseLevel <= 1)
		{
			const CD3D9Renderer::SRenderTileInfo& rti = rd->GetRenderTileInfo();
			if (rti.nGridSizeX>1.f || rti.nGridSizeY>1.f)
			{ // shift and scale viewport
				(*m)._11 *= rti.nGridSizeX;
				(*m)._22 *= rti.nGridSizeY;
				(*m)._41 = -((rti.nGridSizeX-1.f)-rti.nPosX*2.0f);
				(*m)._42 =  ((rti.nGridSizeY-1.f)-rti.nPosY*2.0f);
			}
		}

    rd->PushMatrix();
    rd->m_matView->LoadIdentity();
    pSH->FXSetTechnique("Cloud_ScreenImposter");
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pSH->FXBeginPass(0);

    float fNear = pRE->m_fNear;
    float fFar = pRE->m_fFar;
    if (fNear < 0 || fNear > 1)
      fNear = 0.92f;
    if (fFar < 0 || fFar > 1)
      fFar = 0.999f;
    float fZ = (fNear + fFar) * 0.5f;

		Vec4 pos(pRE->GetPosition(), 1);
		static CCryName vCloudWSPosName("vCloudWSPos");
		pSH->FXSetVSFloat( vCloudWSPosName, &pos, 1 );
		static CCryName g_CloudColorScaleName("g_CloudColorScale");
		pSH->FXSetPSFloat( g_CloudColorScaleName, &vCloudColorScale , 1 );
		
		Vec3 lPos;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_POS, lPos );		
		Vec4 lightningPosition(lPos.x, lPos.y, lPos.z, col.a);		
		static CCryName LightningPosName("LightningPos");
		pSH->FXSetVSFloat( LightningPosName, &lightningPosition, 1 );

		Vec3 lCol;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_COLOR, lCol );
		Vec3 lSize;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_SIZE, lSize );
		Vec4 lightningColorSize( lCol.x, lCol.y, lCol.z, lSize.x * 0.01f );
		static CCryName LightningColSizeName("LightningColSize");
		pSH->FXSetVSFloat( LightningColSizeName, &lightningColorSize, 1 );

		//rd->DrawQuad3D(Vec3(-1,-1,fZ), Vec3(1,-1,fZ), Vec3(1,1,fZ), Vec3(-1,1,fZ), col, 0, 1, 1, 0);
		{
			int nOffs;
			struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *vQuad = (struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *) gcpRendD3D->GetVBPtr( 4, nOffs, POOL_P3F_TEX2F_TEX3F );

			Vec3 vCoords[8];
			gcpRendD3D->GetRCamera().CalcVerts( vCoords );
			Vec3 vRT = vCoords[4] - vCoords[0];
			Vec3 vLT = vCoords[5] - vCoords[1];
			Vec3 vLB = vCoords[6] - vCoords[2];
			Vec3 vRB = vCoords[7] - vCoords[3];

			vQuad[0].p.x = -1;
			vQuad[0].p.y = -1;
			vQuad[0].p.z = fZ;
			vQuad[0].st0[0] = 0;
			vQuad[0].st0[1] = 1;
			vQuad[0].st1 = vLB;

			vQuad[1].p.x = 1;
			vQuad[1].p.y = -1;
			vQuad[1].p.z = fZ;
			vQuad[1].st0[0] = 1;
			vQuad[1].st0[1] = 1;
			vQuad[1].st1 = vRB;

			vQuad[3].p.x = 1;
			vQuad[3].p.y = 1;
			vQuad[3].p.z = fZ;
			vQuad[3].st0[0] = 1;
			vQuad[3].st0[1] = 0;
			vQuad[3].st1 = vRT;

			vQuad[2].p.x = -1;
			vQuad[2].p.y = 1;
			vQuad[2].p.z = fZ;
			vQuad[2].st0[0] = 0;
			vQuad[2].st0[1] = 0;
			vQuad[2].st1 = vLT;

#if defined(OPENGL)
			// XXX Untested!
			vQuad[0].st1 = vLT;
			vQuad[1].st1 = vRT;
			vQuad[2].st1 = vRB;
			vQuad[3].st1 = vLB;
#endif

			rd->UnlockVB( POOL_P3F_TEX2F_TEX3F );

			rd->EF_Commit();
			rd->FX_SetVStream( 0, gcpRendD3D->m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );
			if (!FAILED(rd->EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F_TEX3F )))
      {
  #if defined (DIRECT3D9) || defined (OPENGL)
			  rd->m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
        rd->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        rd->m_pd3dDevice->Draw(4, nOffs);
  #endif
        rd->m_RP.m_PS.m_nPolygons[rd->m_RP.m_nPassGroupDIP] += 2;
        rd->m_RP.m_PS.m_nDIPs[rd->m_RP.m_nPassGroupDIP]++;
      }
		}

    rd->PopMatrix();
    rd->m_matProj->Pop();
    rd->EF_DirtyMatrix();
  }

  pSH->FXEndPass();
  pSH->FXEnd();

  rd->m_RP.m_pCurObject = pObj;
  rd->m_RP.m_pCurInstanceInfo = &rd->m_RP.m_pCurObject->m_II;
  rd->m_RP.m_pShader = pSH;
  rd->m_RP.m_pCurTechnique = pSHT;
  rd->m_RP.m_pCurPass = pPass;
  rd->m_RP.m_FrameObject++;
  rd->m_RP.m_PersFlags &= ~RBPF_WASWORLDSPACE;
  rd->m_RP.m_PrevLMask = -1;

  return true;
}

bool CRECloud::mfDraw(CShader *ef, SShaderPass *pPass)
{
  if (!CRenderer::CV_r_impostersdraw)
    return true;

  CD3D9Renderer *rd = gcpRendD3D;
  CRenderObject *pObj = rd->m_RP.m_pCurObject;
  CREImposter *pRE = (CREImposter *)pObj->m_pRE;

  mfDisplay(false);

  if (pRE->IsSplit())
  {
    // now display the front half of the split impostor.
    mfDisplay(true);
  }

  return true;
}
