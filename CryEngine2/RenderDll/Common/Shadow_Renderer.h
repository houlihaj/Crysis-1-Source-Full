#ifndef SHADOWRENDERER_H
#define SHADOWRENDERER_H

struct ShadowMapFrustum
{
  Matrix44 mLightProjMatrix;
  Matrix44 mLightViewMatrix;
	
	// flags
	bool	bAllowViewDependency;
	bool	bUseVarianceSM;
	bool	bUseAdditiveBlending;

	bool	bForSubSurfScattering;
	
	// if set to true - pCastersList contains all casters in light radius 
	// and all other members related only to single frustum projection case are undefined
	bool	bOmniDirectionalShadow;
	uint8	nOmniFrustumMask;
	uint8	nInvalidatedFrustMask[MAX_GPU_NUM]; //for each GPU

  bool bHWPCFCompare;
  bool bUseHWShadowMap;
  bool bUseFilter;

  float fDepthShift; //precision range shift for FP shadow maps

  bool bNormalizedDepth;

  //sampling parameters 
  f32 fWidthS,fWidthT;
	f32 fBlurS,fBlurT;
  int32 numSamples;

  //temporal solution for frustum bounds
  //they are NDC z coords
  f32 fMinFrustumBound;
  f32 fMaxFrustumBound;

  //test
  Vec3 vMaxBoundPoint;

  //fading distance per light source
  float fShadowFadingDist;

  //3d engine parameters
  struct SDynTexture_Shadow* pDepthTex;
  struct SDynTextureArray* pDepthTexArray;
  float fFOV;
  float fNearDist;
  float fFarDist;
  int   nTexSize;

  //shadow renderer parameters - should be in separate structure
  //atlas parameters
  int   nTextureWidth;
  int   nTextureHeight;
  bool  bUnwrapedOmniDirectional;
  int   nShadowMapSize;
  //int   fShadowMapWidth, fShadowMapHeight;

  int   nResetID;
	float fFrustrumSize;
	float fProjRatio;
	float fDepthTestBias;
  float fDepthConstBias;
  float fDepthSlopeBias;
  PodArray<struct IShadowCaster*> * pCastersList;
	CCamera FrustumPlanes;
	AABB aabbCasters; //casters bbox in world space
	float fCameraFarPlaneGSM; //far plane value for current GSM lod
	Vec3 vLightSrcRelPos; // relative world space
	Vec3 vProjTranslation; // dst position
	float fRadius;
	int nDLightId;
	int nUpdateFrameId;
//	int nAffectsReceiversFrameId;
	IRenderNode * pLightOwner;
	uint uCastersListCheckSum;
	int nShadowMapLod;			// currently use as GSMLod, can be used as cubemap side, -1 means this variable is not used

//	float fMaxDistanceOfTheRecivers[6];														//max distances / face (if it's a cubemap) for determine which object we needed
//	float f_OldMaxDistanceOfTheRecivers[6];														//max distances / face (if it's a cubemap) for determine which object we needed
	uint	m_Flags;

  ShadowMapFrustum()
  {
    ZeroStruct(*this);
		fProjRatio = 1.f;
		nDLightId = -1;

    fMinFrustumBound = 0.0f;
    fMaxFrustumBound = 1.0f;

    //initial frustum position should be outside of the visible map
    vProjTranslation = Vec3(-1000.0f, -1000.0f, -1000.0f);

    bUnwrapedOmniDirectional = false;
    nShadowMapSize = 0;

  }

  ~ShadowMapFrustum()
  {
    delete pCastersList;
    //SAFE_DELETE((IDynTexture*)pDepthTexArray);
    //SAFE_RELEASE((IDynTexture*)pDepthTex);
  }

  void GetSideViewport( int nSide, int* pViewport)
  {
    //simplest cubemap 6 faces unwrap
    pViewport[0] = nShadowMapSize * (nSide%3);
    pViewport[1] = nShadowMapSize * (nSide/3);
    pViewport[2] = nShadowMapSize;
    pViewport[3] = nShadowMapSize;
  }

  void GetTexOffset( int nSide, float* pOffset)
  {
    pOffset[0] = 1.0f/3.0f * (nSide%3);
    pOffset[1] = 1.0f/2.0f * (nSide/3);
  }

	void RequestUpdate()
	{
    for (int i=0; i<MAX_GPU_NUM; i++)
  		nInvalidatedFrustMask[i] = 0x3F;
	}

	bool isUpdateRequested(int nMaskNum)
	{
    assert(nMaskNum<MAX_GPU_NUM);
		return (nInvalidatedFrustMask[nMaskNum]>0);
	}

	bool Intersect(const AABB & bbox)
	{
		if(bOmniDirectionalShadow)
			return bbox.IsOverlapSphereBounds(vLightSrcRelPos+vProjTranslation, fFarDist);
		else
			return FrustumPlanes.IsAABBVisible_E(bbox);
	}

  void UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, IRenderer * pRend)
  {
    const int shadowViewport[4] = {0,0,1,1};
			
		//FIX remove float arrays
    pRend->UnProject(sx,sy,sz,
      px,py,pz,
      (float*)&mLightViewMatrix,
      (float*)&mLightProjMatrix,
      shadowViewport);
  }

  Vec3 & UnProjectVertex3d(int sx, int sy, int sz, Vec3 & vert, IRenderer * pRend)
  {
    float px; 
    float py; 
    float pz;
    UnProject((float)sx, (float)sy, (float)sz, &px, &py, &pz, pRend);
    vert.x=(float)px;
    vert.y=(float)py;
    vert.z=(float)pz;

//		pRend->DrawBall(vert,10);

    return vert;
  }
  
  void DrawFrustum(IRenderer * pRend)
  {
		if(abs(nUpdateFrameId - pRend->GetFrameID())>1)
			return;

		//if(!arrLightViewMatrix[0] && !arrLightViewMatrix[5] && !arrLightViewMatrix[10])
			//return;

		IRenderAuxGeom * pRendAux = pRend->GetIRenderAuxGeom();
  
    Vec3 vert1,vert2;
		ColorB c0(255,255,255,255);
    {
      pRendAux->DrawLine(
        UnProjectVertex3d(0,0,0,vert1,pRend), c0,
        UnProjectVertex3d(0,0,1,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(1,0,0,vert1,pRend), c0,
        UnProjectVertex3d(1,0,1,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(1,1,0,vert1,pRend), c0,
        UnProjectVertex3d(1,1,1,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(0,1,0,vert1,pRend), c0,
        UnProjectVertex3d(0,1,1,vert2,pRend), c0);
    }

    for(int i=0; i<=1; i++)
    {
      pRendAux->DrawLine(
        UnProjectVertex3d(0,0,i,vert1,pRend), c0,
        UnProjectVertex3d(1,0,i,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(1,0,i,vert1,pRend), c0,
        UnProjectVertex3d(1,1,i,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(1,1,i,vert1,pRend), c0,
        UnProjectVertex3d(0,1,i,vert2,pRend), c0);

      pRendAux->DrawLine(
        UnProjectVertex3d(0,1,i,vert1,pRend), c0,
        UnProjectVertex3d(0,0,i,vert2,pRend), c0);
    }
  }
};

#endif
