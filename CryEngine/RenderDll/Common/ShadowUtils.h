/*=============================================================================
  ShadowUtils.h : 
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/

#ifndef __SHADOWUTILS_H__
#define __SHADOWUTILS_H__

#define DEG2RAD_R( a ) ( f64(a) * (g_PI/180.0) )

class CPoissonDiskGen
{
	static bool		m_bCached;
	static int		m_numSamples;
	static Vec2*	m_pvSamples;

private:
	static void RandomPoint(Vec2& p);
	static void InitSamples();

public:
	static void SetKernelSize(int num);
	static Vec2& GetSample(int ind);

	CPoissonDiskGen();
	~CPoissonDiskGen();
};

enum EFrustum_Type
{
	FTYP_SHADOWOMNIPROJECTION,
	FTYP_SHADOWPROJECTION,
	FTYP_OMNILIGHTVOLUME,
	FTYP_LIGHTVOLUME,
	FTYP_MAX,
	FTYP_UNKNOWN
};

class CShadowUtils
{
public:
  static float g_fOmniShadowFov;
  static float g_fOmniLightFov;

public:
	static void ProjectScreenToWorldExpansionBasis(const Matrix44r& mShadowTexGen, const CCamera& cam, float fViewWidth, float fViewHeight, Vec4r& vWBasisX, Vec4r& vWBasisY, Vec4r& vWBasisZ, Vec4r& vCamPos, bool bWPos, CRenderer::SRenderTileInfo* pSTileInfo);
  static void CalcScreenToWorldExpansionBasis(const CCamera& cam, float fViewWidth, float fViewHeight, Vec3& vWBasisX, Vec3& vWBasisY, Vec3& vWBasisZ, bool bWPos);

  static void CalcLightBoundRect(CDLight* pLight, const CRenderCamera& RCam, Matrix44& mView, Matrix44& mProj, Vec2* pvMin,  Vec2* pvMax, IRenderAuxGeom* pAuxRend);
  static void GetCubemapFrustumForLight(CDLight* pLight, int nS, float fFov, Matrix44* pmProj, Matrix44* pmView, bool bShadowGen);

  static void GetCubemapFrustum(EFrustum_Type eFrustumType, ShadowMapFrustum* pFrust, int nS, Matrix44* pmProj, Matrix44* pmView, Matrix33* pmLightRot=NULL);

  static void mathMatrixLookAtSnap(Matrix44* pMatr, const Vec3& Eye, const Vec3& At, ShadowMapFrustum* pFrust);
  static void GetShadowMatrixOrtho(Matrix44& mLightProj, Matrix44& mLightView, const Matrix44& mViewMatrix, ShadowMapFrustum* lof, bool bViewDependent);


  CShadowUtils();
  ~CShadowUtils();

};

#endif