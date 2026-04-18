/*=============================================================================
  ShadowUtils.cpp : 
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "StdAfx.h"
#include "ShadowUtils.h"

bool CPoissonDiskGen::m_bCached = false;
int CPoissonDiskGen::m_numSamples = 0;
Vec2* CPoissonDiskGen::m_pvSamples = NULL;

float CShadowUtils::g_fOmniShadowFov = 95.0f;
float CShadowUtils::g_fOmniLightFov = 90.5f;

void CShadowUtils::ProjectScreenToWorldExpansionBasis(const Matrix44r& mShadowTexGen, const CCamera& cam, float fViewWidth, float fViewHeight, Vec4r& vWBasisX, Vec4r& vWBasisY, Vec4r& vWBasisZ, Vec4r& vCamPos, bool bWPos, CRenderer::SRenderTileInfo* pSTileInfo)
{

	const Matrix34& camMatrix = cam.GetMatrix();

	Vec3 vNearEdge = cam.GetEdgeN();

	//all values are in camera space
	float fFar = cam.GetFarPlane();
	float fNear	= abs(vNearEdge.y);
	float fWorldWidthDiv2 = abs(vNearEdge.x);
	float fWorldHeightDiv2 = abs(vNearEdge.z); 

	float k = fFar/fNear;

	Vec3 vZ = camMatrix.GetColumn1().GetNormalized() * fNear * k; // size of vZ is the distance from camera pos to near plane
	Vec3 vX = camMatrix.GetColumn0().GetNormalized() * fWorldWidthDiv2 * k;
	Vec3 vY = camMatrix.GetColumn2().GetNormalized() * fWorldHeightDiv2 * k;

	//Add view position to constant vector to avoid additional ADD in the shader
	//vZ = vZ + cam.GetPosition();

	//multi-tiled render handling
	if (pSTileInfo)
	{
		vZ = vZ + ( vX*(2.0 * (pSTileInfo->nGridSizeX-1.0-pSTileInfo->nPosX)/pSTileInfo->nGridSizeX) );
		vZ = vZ - ( vY*(2.0 * (pSTileInfo->nGridSizeY-1.0-pSTileInfo->nPosY)/pSTileInfo->nGridSizeY) );
	}

	//TFIX calc how k-scale does work with this projection
	//WPos basis adjustments
	if (bWPos)
	{

		//float fViewWidth = cam.GetViewSurfaceX();
		//float fViewHeight = cam.GetViewSurfaceZ();
		vZ = vZ - vX;
		vX *= (2.0f/fViewWidth);
		//vX *= 2.0f; //uncomment for ScreenTC

		vZ = vZ + vY;
		vY *= -(2.0f/fViewHeight);
		//vY *= -2.0f; //uncomment for ScreenTC
	}

	//multi-tiled render handling
	if (pSTileInfo)
	{
		vX *= 1.0f/pSTileInfo->nGridSizeX;
		vY *= 1.0f/pSTileInfo->nGridSizeY;
	}

	//TOFIX: create PB components for these params
	//creating common projection matrix for depth reconstruction
	vWBasisX = mShadowTexGen * Vec4r(vX, 0.0f);
	vWBasisY = mShadowTexGen * Vec4r(vY, 0.0f);
	vWBasisZ = mShadowTexGen * Vec4r(vZ, 0.0f);
	vCamPos =  mShadowTexGen * Vec4r(cam.GetPosition(), 1.0f);

}


void CShadowUtils::CalcScreenToWorldExpansionBasis(const CCamera& cam, float fViewWidth, float fViewHeight, Vec3& vWBasisX, Vec3& vWBasisY, Vec3& vWBasisZ, bool bWPos)
{

  const Matrix34& camMatrix = cam.GetMatrix();

  Vec3 vNearEdge = cam.GetEdgeN();

  //all values are in camera space
  float fFar = cam.GetFarPlane();
  float fNear	= abs(vNearEdge.y);
  float fWorldWidthDiv2 = abs(vNearEdge.x);
  float fWorldHeightDiv2 = abs(vNearEdge.z); 

  float k = fFar/fNear;

  Vec3 vZ = camMatrix.GetColumn1().GetNormalized() * (fNear * k); // size of vZ is the distance from camera pos to near plane
  Vec3 vX = camMatrix.GetColumn0().GetNormalized() * (fWorldWidthDiv2 * k);
  Vec3 vY = camMatrix.GetColumn2().GetNormalized() * (fWorldHeightDiv2 * k);

  //Add view position to constant vector to avoid additional ADD in the shader
  //vZ = vZ + cam.GetPosition();

  //WPos basis adjustments
  if (bWPos)
  {
    //float fViewWidth = cam.GetViewSurfaceX();
    //float fViewHeight = cam.GetViewSurfaceZ();

    vZ = vZ - vX;
    vX *= (2.0f/fViewWidth);
    //vX *= 2.0f; //uncomment for ScreenTC

    vZ = vZ + vY;
    vY *= -(2.0f/fViewHeight);
    //vY *= -2.0f; //uncomment for ScreenTC
  }

  vWBasisX = vX;
  vWBasisY = vY;
  vWBasisZ = vZ;

}

void CShadowUtils::CalcLightBoundRect(CDLight* pLight, const CRenderCamera& RCam, Matrix44& mView, Matrix44& mProj, Vec2* pvMin,  Vec2* pvMax, IRenderAuxGeom* pAuxRend)
{
  Vec3 vViewVec = Vec3(pLight->m_Origin - RCam.Orig);
  float fDistToLS =  vViewVec.GetLength();

  if (fDistToLS<=pLight->m_fRadius)
  {
    //optimization when we are inside light frustum
    *pvMin = Vec2(0,0);
    *pvMax = Vec2(1,1);
    return;
  }


  float fRadiusSquared = pLight->m_fRadius * pLight->m_fRadius;

  //distance from Light Source center to the bound plane
  float fDistToBoundPlane = fRadiusSquared/fDistToLS;

  float fQuadEdge = sqrt(fRadiusSquared - fDistToBoundPlane * fDistToBoundPlane);

  vViewVec.SetLength( fDistToLS - fDistToBoundPlane );

  Vec3 vCenter = RCam.Orig +  vViewVec;

  Vec3 vUp = vViewVec.Cross(RCam.Y.Cross(vViewVec));
  Vec3 vRight = vViewVec.Cross(RCam.X.Cross(vViewVec));

  vUp.normalize();
  vRight.normalize();

  float fRadius = pLight->m_fRadius;

  Vec3 pBRectVertices[4] =
  {
    vCenter + (vUp*fQuadEdge) -  (vRight*fQuadEdge),
    vCenter + (vUp*fQuadEdge) +  (vRight*fQuadEdge),
    vCenter - (vUp*fQuadEdge) +  (vRight*fQuadEdge),
    vCenter - (vUp*fQuadEdge) -  (vRight*fQuadEdge)
  };

  Vec3 pBBoxVertices[8] =
  {
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3(-fRadius,-fRadius, fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3(-fRadius, fRadius, fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3( fRadius,-fRadius, fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3( fRadius, fRadius, fRadius)),

    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3(-fRadius,-fRadius, -fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3(-fRadius, fRadius, -fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3( fRadius,-fRadius, -fRadius)),
    Vec3(Vec3(vCenter.x, vCenter.y, vCenter.z) + Vec3( fRadius, fRadius, -fRadius))
  };

  //TODO: cache inverted mitrices for all deff meshes reconstructions
  Matrix44 mInvView = mView.GetInverted();

  *pvMin = Vec2(1,1);
  *pvMax = Vec2(0,0);

  for (int i = 0; i<4; i++)
  {
    if (pAuxRend!=NULL)
    {
      pAuxRend->DrawPoint( pBRectVertices[i] ,RGBA8(0xff,0xff,0xff,0xff),10);

      int32 nPrevVert = (i-1)<0?3:(i-1);

      pAuxRend->DrawLine( pBRectVertices[nPrevVert], RGBA8(0xff,0xff,0x0,0xff), pBRectVertices[i], RGBA8(0xff,0xff,0x0,0xff), 3.0f);
    }


    Vec4 vScreenPoint = Vec4(pBRectVertices[i], 1.0) * mProj;

    //projection space clamping
    vScreenPoint.w = max(vScreenPoint.w, 0.00000000000001f);

    vScreenPoint.x = max(vScreenPoint.x, -(vScreenPoint.w));
    vScreenPoint.x = min(vScreenPoint.x, vScreenPoint.w);
    vScreenPoint.y = max(vScreenPoint.y, -(vScreenPoint.w));
    vScreenPoint.y = min(vScreenPoint.y, vScreenPoint.w);


    //NDC
    vScreenPoint /= vScreenPoint.w;

    //output coords
    //generate viewport (x=0,y=0,height=1,width=1)
    Vec2 vWin;
    vWin.x = (1 + vScreenPoint.x)/ 2;
    vWin.y = (1 + vScreenPoint.y)/ 2;  //flip coords for y axis

    assert(vWin.x>=0.0f && vWin.x<=1.0f);
    assert(vWin.y>=0.0f && vWin.y<=1.0f);

    pvMin->x = min( pvMin->x,vWin.x );
    pvMin->y = min( pvMin->y,vWin.y );
    pvMax->x = max( pvMax->x,vWin.x );
    pvMax->y = max( pvMax->y,vWin.y );
  }
}

//TF: make support for cubemap faces
void CShadowUtils::GetCubemapFrustumForLight(CDLight* pLight, int nS, float fFov, Matrix44* pmProj, Matrix44* pmView, bool bShadowGen)
{

  //new cubemap calculation
  float sCubeVector[6][7] = 
  {
    {1,0,0,  0,0,-1, -90}, //posx
    {-1,0,0, 0,0,1,  90}, //negx
    {0,1,0,  -1,0,0, 0},  //posy
    {0,-1,0, 1,0,0,  0},  //negy
    {0,0,1,  0,-1,0,  0},  //posz
    {0,0,-1, 0,1,0,  0},  //negz 
  };

  /*const float sCubeVector[6][7] = 
  {
    {1,0,0,  0,0,1, -90}, //posx
    {-1,0,0, 0,0,1,  90}, //negx
    {0,1,0,  0,0,-1, 0},  //posy
    {0,-1,0, 0,0,1,  0},  //negy
    {0,0,1,  0,1,0,  0},  //posz
    {0,0,-1, 0,1,0,  0},  //negz 
  };*/

  Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
  Vec3 vUp      = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
  Vec3 vEyePt = pLight->m_Origin;
  vForward = vForward + vEyePt;

  //mathMatrixLookAt( pmView, vEyePt, vForward, vUp );

  //adjust light rotation
  Matrix34 mLightRot = pLight->m_ObjMatrix;

  //coord systems conversion(from orientation to shader matrix)
  Vec3 zaxis = mLightRot.GetColumn2().GetNormalized();
  Vec3 yaxis = -mLightRot.GetColumn0().GetNormalized();
  Vec3 xaxis = mLightRot.GetColumn1().GetNormalized();

  //Vec3 vLightDir = xaxis;

  //RH
  (*pmView)(0,0) = xaxis.x;		(*pmView)(0,1) = zaxis.x;	(*pmView)(0,2) = yaxis.x;	(*pmView)(0,3) = 0; 
  (*pmView)(1,0) = xaxis.y;		(*pmView)(1,1) = zaxis.y;	(*pmView)(1,2) = yaxis.y;	(*pmView)(1,3) = 0; 
  (*pmView)(2,0) = xaxis.z;		(*pmView)(2,1) = zaxis.z;	(*pmView)(2,2) = yaxis.z;	(*pmView)(2,3) = 0; 
  (*pmView)(3,0) = -xaxis.Dot(vEyePt);	(*pmView)(3,1) = -zaxis.Dot(vEyePt);	(*pmView)(3,2) = -yaxis.Dot(vEyePt);	(*pmView)(3,3) = 1; 

  mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(fFov), 1.f, max(pLight->m_fProjectorNearPlane,0.1f), pLight->m_fRadius);

  /*if (bShadowGen)
  {
    mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(fFov), 1.f, max(pLight->m_fProjectorNearPlane,0.01f), pLight->m_fRadius);
  }
  else
  {
    mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(fFov*1.001), 1.f, max(pLight->m_fProjectorNearPlane,0.01f)+0.01f, pLight->m_fRadius+3.0f);
  }*/
}



void CShadowUtils::GetCubemapFrustum(EFrustum_Type eFrustumType, ShadowMapFrustum* pFrust, int nS, Matrix44* pmProj, Matrix44* pmView, Matrix33* pmLightRot)
{
  //new cubemap calculation

  float sCubeVector[6][7] = 
  {
    {1,0,0,  0,0,-1, -90}, //posx
    {-1,0,0, 0,0,1,  90}, //negx
    {0,1,0,  -1,0,0, 0},  //posy
    {0,-1,0, 1,0,0,  0},  //negy
    {0,0,1,  0,-1,0,  0},  //posz
    {0,0,-1, 0,1,0,  0},  //negz 
  };

  /*const float sCubeVector[6][7] = 
  {
    {1,0,0,  0,0,1, -90}, //posx
    {-1,0,0, 0,0,1,  90}, //negx
    {0,1,0,  0,0,-1, 0},  //posy
    {0,-1,0, 0,0,1,  0},  //negy
    {0,0,1,  0,1,0,  0},  //posz
    {0,0,-1, 0,1,0,  0},  //negz 
  };*/

  Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
  Vec3 vUp      = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
  float fMinDist = max(pFrust->fNearDist, 0.03f);
  float fMaxDist = pFrust->fFarDist;

  Vec3 vEyePt = Vec3(
    pFrust->vLightSrcRelPos.x + pFrust->vProjTranslation.x,
    pFrust->vLightSrcRelPos.y + pFrust->vProjTranslation.y,
    pFrust->vLightSrcRelPos.z + pFrust->vProjTranslation.z
    );
  Vec3 At = vEyePt;

  if (eFrustumType==FTYP_OMNILIGHTVOLUME)
  {
    vEyePt -= (2.0 * fMinDist) * vForward.GetNormalized();
  }

  vForward = vForward + At;

  //mathMatrixTranslation(&mLightTranslate, -vPos.x, -vPos.y, -vPos.z);2
  mathMatrixLookAt( pmView, vEyePt, vForward, vUp );

  //adjust light rotation
  if (pmLightRot)
  {
    (*pmView) = (*pmView) * (*pmLightRot);
  }
	if (eFrustumType==FTYP_SHADOWOMNIPROJECTION)
	{
		//near plane does have big influence on the precision (depth distribution) for non linear frustums
		mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(g_fOmniShadowFov), 1.f, fMinDist, fMaxDist );
	}
	else
	if (eFrustumType==FTYP_OMNILIGHTVOLUME)
	{
		//near plane should be extremely small in order to avoid  seams on between frustums
		mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(g_fOmniLightFov), 1.f, fMinDist, fMaxDist );
	}

}

static _inline Vec3 frac(Vec3 in)
{
  Vec3 out;
  out.x = in.x - (float)(int)in.x;
  out.y = in.y - (float)(int)in.y;
  out.z = in.z - (float)(int)in.z;

  return out;
}

float snap_frac2(float fVal, float fSnap)
{	
  float fValSnapped = fSnap*int64(fVal/fSnap);
  return fValSnapped;
}

//Right Handed
void CShadowUtils::mathMatrixLookAtSnap(Matrix44* pMatr, const Vec3& Eye, const Vec3& At, ShadowMapFrustum* pFrust)
{
  const Vec3 vZ(0.f, 0.f, 1.f);
  const Vec3 vY(0.f, 1.f, 0.f);

  Vec3 vUp;
  Vec3 vEye = Eye;
  Vec3 vAt = At;

  Vec3 vLightDir = (vEye - vAt);
  vLightDir.Normalize();

  if ( fabsf(vLightDir.Dot(vZ))>0.9995f )
    vUp = vY;
  else
    vUp = vZ;

  float fKatetSize = 1000000.0f  * cry_tanf(DEG2RAD(pFrust->fFOV)*0.5f);

  assert(pFrust->nTexSize>0);
  float fSnapXY = fKatetSize * 2.f / pFrust->nTexSize; //texture size should be valid already
  fSnapXY *= 16.0f;

  float fZSnap = 192.0f*2.0f / 16777216.f/*1024.0f*/;
  //fZSnap *= 32.0f;

  Vec3 zaxis = vLightDir.GetNormalized();
  Vec3 xaxis = (vUp.Cross(zaxis)).GetNormalized();
  Vec3 yaxis =zaxis.Cross(xaxis);

  //vAt -= xaxis * snap_frac(vAt.Dot(xaxis), fSnapXY);
  //vAt -= yaxis  * snap_frac(vAt.Dot(yaxis), fSnapXY);

  (*pMatr)(0,0) = xaxis.x;		(*pMatr)(0,1) = yaxis.x;	(*pMatr)(0,2) = zaxis.x;	(*pMatr)(0,3) = 0; 
  (*pMatr)(1,0) = xaxis.y;		(*pMatr)(1,1) = yaxis.y;	(*pMatr)(1,2) = zaxis.y;	(*pMatr)(1,3) = 0; 
  (*pMatr)(2,0) = xaxis.z;		(*pMatr)(2,1) = yaxis.z;	(*pMatr)(2,2) = zaxis.z;	(*pMatr)(2,3) = 0; 
  (*pMatr)(3,0) = -xaxis.Dot(vEye);	(*pMatr)(3,1) = -yaxis.Dot(vEye);	(*pMatr)(3,2) = -zaxis.Dot(vEye);	(*pMatr)(3,3) = 1; 

  float fTranslX = (*pMatr)(3,0);
  float fTranslY = (*pMatr)(3,1);
  float fTranslZ = (*pMatr)(3,2);

  (*pMatr)(3,0) = snap_frac2(fTranslX, fSnapXY);
  (*pMatr)(3,1) = snap_frac2(fTranslY, fSnapXY);
  //(*pMatr)(3,2) = snap_frac2(fTranslZ, fZSnap);

}

//todo move frustum computations to the 3d engine
void CShadowUtils::GetShadowMatrixOrtho(Matrix44& mLightProj, Matrix44& mLightView, const Matrix44& mViewMatrix, ShadowMapFrustum* lof, bool bViewDependent)
{

  mathMatrixPerspectiveFov(&mLightProj, DEG2RAD_R(lof->fFOV), lof->fProjRatio, lof->fNearDist, lof->fFarDist);

  //linearized depth
  //mLightProj.m22/= lof->fFarDist;
  //mLightProj.m32/= lof->fFarDist;

  const Vec3 zAxis(0.f, 0.f, 1.f);
  const Vec3 yAxis(0.f, 1.f, 0.f);
  Vec3 Up;
  Vec3 Eye = Vec3(
    lof->vLightSrcRelPos.x+lof->vProjTranslation.x, 
    lof->vLightSrcRelPos.y+lof->vProjTranslation.y, 
    lof->vLightSrcRelPos.z+lof->vProjTranslation.z);
  Vec3 At = Vec3(lof->vProjTranslation.x, lof->vProjTranslation.y, lof->vProjTranslation.z);

  Vec3 vLightDir = At - Eye;
  vLightDir.Normalize();

  if (bViewDependent)
  {
    //we should have LightDir vector transformed to the view space

    Eye = GetTransposed44(mViewMatrix).TransformPoint(Eye);
    At = GetTransposed44(mViewMatrix).TransformPoint(At);

    vLightDir = GetTransposed44(mViewMatrix).TransformVector(vLightDir);
    //vLightDir.Normalize();

  }

  //get look-at matrix
  if (CRenderer::CV_r_ShadowsGridAligned && lof->m_Flags & DLF_DIRECTIONAL)
  {
    mathMatrixLookAtSnap(&mLightView, Eye, At, lof);
  }
  else
  {
    if ( fabsf(vLightDir.Dot(zAxis))>0.9995f )
      Up = yAxis;
    else
      Up = zAxis;

    mathMatrixLookAt(&mLightView, Eye, At, Up);
  }

  //we should transform coords to the view space, so shadows are oriented according to camera always
  if (bViewDependent)
  {
    mLightView = mViewMatrix * mLightView;
  }
}



CShadowUtils::CShadowUtils()
{

}

CShadowUtils::~CShadowUtils()
{

}


CPoissonDiskGen::CPoissonDiskGen()
{

}
CPoissonDiskGen::~CPoissonDiskGen()
{

}

Vec2& CPoissonDiskGen::GetSample(int ind)
{
	assert(ind<m_numSamples && ind>=0);

	return 
		m_pvSamples[ind];
}

void CPoissonDiskGen::SetKernelSize(int num)
{
	if (m_numSamples != num && num > 0)
	{
		m_numSamples = num;

		SAFE_DELETE_ARRAY(m_pvSamples);

		m_pvSamples = new Vec2[m_numSamples];

		InitSamples();
	}

	return;
}

void CPoissonDiskGen::RandomPoint(Vec2& p)
{
	//generate random point inside circle
	do {
			p.x = rnd()- 0.5;
			p.y = rnd()- 0.5;
	} while (p.x*p.x + p.y*p.y > 0.25);

	return;
}


void CPoissonDiskGen::InitSamples()
{
	const int nQ = 1000;

	RandomPoint(m_pvSamples[0]);

	for (int i = 1; i < m_numSamples; i++)
	{
		float dmax = -1.0;

		for (int c = 0; c < i * nQ; c++)
		{
			Vec2 curSample;
			RandomPoint (curSample);

			float dc = 2.0;
			for (int j = 0; j < i; j++)
			{
				float dj =
					(m_pvSamples[j].x - curSample.x) * (m_pvSamples[j].x - curSample.x) +
					(m_pvSamples[j].y - curSample.y) * (m_pvSamples[j].y - curSample.y);
				if (dc > dj)
					dc = dj;
			}

			if (dc > dmax) 
			{
				m_pvSamples[i] = curSample;
				dmax = dc;
			}
		}
	}

  for (int i = 0; i < m_numSamples; i++)
  {
    m_pvSamples[i] *=2.0f;
    //CryLogAlways("Sample %i: (%.6f, %.6f)\n", i, m_pvSamples[i].x, m_pvSamples[i].y);
  }

	return;
}