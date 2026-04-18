#include "StdAfx.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain.h"
#include "LightEntity.h"
#include "CullBuffer.h"
#include "ICryAnimation.h"

#define TEXTURE_SIZE_FOR_VERTEX_SCATTERING_CAPTURE 2048

Vec3 CLightEntity::GetPos(bool bWorldOnly) const
{
	assert(bWorldOnly);
	return m_light.m_Origin; 
}

float CLightEntity::GetMaxViewDist()
{
  if(m_light.m_Flags&DLF_SUN)
    return 10.f*DISTANCE_TO_THE_SUN;

  if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
    return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

  return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio*GetViewDistRatioNormilized());
}
void CLightEntity::InitEntityShadowMapInfoStructure()
{
	// Init ShadowMapInfo structure
	if(!m_pShadowMapInfo)
		m_pShadowMapInfo = new ShadowMapInfo(); // leak
}

CLightEntity::CLightEntity()
{
	m_pShadowMapInfo = NULL;
  m_pNotCaster = NULL;

	memset(m_arrCB,0,sizeof(m_arrCB));
	memset(&m_Matrix,0,sizeof(m_Matrix));

	m_bCoverageBufferDirty = true;
}

CLightEntity::~CLightEntity()
{
	m_light.m_Shader = NULL; // hack to avoid double deallocation

	Get3DEngine()->UregisterLightFromAccessabilityCache(this);
	Get3DEngine()->FreeRenderNodeState(this);
	((C3DEngine*)Get3DEngine())->FreeLightSourceComponents( &m_light );
	Get3DEngine()->UnRegisterEntity(this);
	((C3DEngine*)Get3DEngine())->RemoveEntityLightSources(this);

	for(int i=0; i<6; i++)
		delete m_arrCB[i];
}

const char *CLightEntity::GetName(void) const 
{ 
	return (m_light.m_Flags&DLF_SUN) ? "Sun" : "LightEntityName"; 
}

bool CLightEntity::IsLightAreasVisible()
{
	IVisArea * pArea = GetEntityVisArea();

	// test area vis
	if(!pArea || pArea->GetVisFrameId() == GetRenderer()->GetFrameID())
		return true; // visible

	if(m_light.m_Flags & DLF_THIS_AREA_ONLY)
		return false;

	// test neighbors
	IVisArea * Areas[64];
	int nCount = pArea->GetVisAreaConnections(Areas,64);
	for (int i=0; i<nCount; i++)
		if(Areas[i]->GetVisFrameId() == GetRenderer()->GetFrameID())
			return true; // visible

	return false; // not visible
}

//////////////////////////////////////////////////////////////////////////
void CLightEntity::SetMatrix( const Matrix34& mat )
{
	if(!memcmp(&m_Matrix, &mat, sizeof(Matrix34)))
		return;

	m_Matrix = mat;
	Vec3 wp = mat.GetTranslation();
	float fRadius = m_light.m_fRadius;
	SetBBox( AABB(Vec3(wp.x-fRadius,wp.y-fRadius,wp.z-fRadius),Vec3(wp.x+fRadius,wp.y+fRadius,wp.z+fRadius)) );
	m_light.m_Origin = wp;
	SetLightProperties(m_light);
	Get3DEngine()->RegisterEntity( this );

  //update shadow frustums
  if (m_pShadowMapInfo!=NULL)
  {
    for(int i=0;i<MAX_GSM_LODS_NUM && m_pShadowMapInfo->pGSM[i]!=NULL ;i++ )
    {
      m_pShadowMapInfo->pGSM[i]->RequestUpdate();
    }
  }


}


void C3DEngine::UpdateSunLightSource()
{
	if(!m_pSun)
		m_pSun = (CLightEntity *)CreateLightSource();

	CDLight DynLight;

//	float fGSMBoxSize = (float)GetCVars()->e_gsm_range;
//	Vec3 vCameraDir = GetCamera().GetMatrix().GetColumn(1).GetNormalized();
//	Vec3 vSunDir = Get3DEngine()->GetSunDir().GetNormalized();		// todo: remove GetNormalized() once GetSunDir() returns the normalized value
	//Vec3 vCameraDirWithoutDepth = vCameraDir - vCameraDir.Dot(vSunDir)*vSunDir;
//	Vec3 vFocusPos = GetCamera().GetPosition() + vCameraDirWithoutDepth*fGSMBoxSize;

	DynLight.m_Origin = GetCamera().GetPosition() + GetSunDir();
	DynLight.m_fRadius  = 100000000;
	DynLight.SetLightColor(GetSunColor());
	DynLight.m_SpecMult = GetSunSpecMultiplier();
	DynLight.m_Flags |= DLF_DIRECTIONAL | DLF_SUN | DLF_THIS_AREA_ONLY | DLF_LM | DLF_SPECULAROCCLUSION | 
		((m_bSunShadows && GetCVars()->e_shadows) ? DLF_CASTSHADOW_MAPS : 0);
	DynLight.m_sName = "Sun";

	m_pSun->SetLightProperties(DynLight);

	m_pSun->SetBBox(AABB(
		DynLight.m_Origin-Vec3(DynLight.m_fRadius,DynLight.m_fRadius,DynLight.m_fRadius),
		DynLight.m_Origin+Vec3(DynLight.m_fRadius,DynLight.m_fRadius,DynLight.m_fRadius)));

	m_pSun->SetRndFlags(ERF_OUTDOORONLY, true);

	m_nRenderStackLevel--;

	if(GetCVars()->e_sun)
		RegisterEntity(m_pSun);
  else
    UnRegisterEntity(m_pSun);

	m_nRenderStackLevel++;
}

Vec3 CLightEntity::GSM_GetNextScreenEdge(float fPrevRadius, float fPrevDistanceFromView)
{
  //////////////////////////////////////////////////////////////////////////
  //Camera Frustum edge
  Vec3 vEdgeN = GetCamera().GetEdgeN();
  Vec3 vEdgeF = GetCamera().GetEdgeF();

  //Lineseg lsgFrustumEdge(vEdgeN, vEdgeF);

  Vec3 vPrevSphereCenter(0.0f ,fPrevDistanceFromView, 0.0f);

  //float fPointT;
  //float fDistToFrustEdge = ( (vPrevSphereCenter.Cross(vEdgeF)).GetLength() ) / ( vEdgeF.GetLength() );

  float fDistToFrustEdge = ( ((vPrevSphereCenter-vEdgeN).Cross(vEdgeF-vEdgeN)).GetLength() ) / ( (vEdgeF-vEdgeN).GetLength() );

  /*Vec3 diff = vPrevSphereCenter - vEdgeN;
  Vec3 dir = vEdgeF - vEdgeN;
  fPointT = (diff.Dot(dir))/dir.GetLength();*/

  //Vec3 vProjectedCenter = lsgFrustumEdge.GetPoint(fPointT);
  
  //float fDistToFrustEdge = (vProjectedCenter - vPrevSphereCenter).GetLength();

	float fDistToPlaneOnEdgeSquared = fPrevRadius*fPrevRadius - fDistToFrustEdge*fDistToFrustEdge;
	float fDistToPlaneOnEdge = fDistToPlaneOnEdgeSquared > 0.0f ? cry_sqrtf(fDistToPlaneOnEdgeSquared) : 0.0f;

  Vec3 vEdgeDir = (vEdgeF - vEdgeN);

  vEdgeDir.SetLength(2.0f * fDistToPlaneOnEdge);
  
  Vec3 vEdgeNextScreen = vEdgeN /*vProjectedCenter*/ + vEdgeDir;


  return vEdgeNextScreen;
};

float CLightEntity::GSM_GetLODProjectionCenter(const Vec3& vEdgeScreen, float fRadius)
{
  float fDistanceFromNear = cry_sqrtf( fRadius*fRadius - vEdgeScreen.z*vEdgeScreen.z - vEdgeScreen.x*vEdgeScreen.x );
  float fDistanceFromView = fDistanceFromNear + vEdgeScreen.y;

  return fDistanceFromView;
}


void CLightEntity::UpdateGSMLightSourceShadowFrustum()
{
	assert(m_pTerrain);
	InitEntityShadowMapInfoStructure();

	float fGSMBoxSize = (float)GetCVars()->e_gsm_range;
	Vec3 vCameraDir = GetCamera().GetMatrix().GetColumn(1).GetNormalized();
	float fDistToLight = GetCamera().GetPosition().GetDistance(GetPos(true));

	static PodArray<SPlaneObject> lstCastersHull; lstCastersHull.Clear();


	// prepare shadow frustums
	int nLod;

  //compute distance for first LOD
  Vec3 vEdgeScreen = GetCamera().GetEdgeN();
  //clamp first frustum to DRAW_NEAREST_MIN near plane because weapon can be placed beyond camera near plane in world space
  vEdgeScreen.y = min(vEdgeScreen.y, DRAW_NEAREST_MIN);
  float fDistanceFromView = GSM_GetLODProjectionCenter(vEdgeScreen, GetCVars()->e_gsm_range);


	for(nLod=0; nLod<GetCVars()->e_gsm_lods_num && nLod<MAX_GSM_LODS_NUM; nLod++)
	{
		float fFOV = (m_light).m_fLightFrustumAngle*2;
		bool bDoGSM = (fGSMBoxSize < m_light.m_fRadius*0.01f && fGSMBoxSize < fDistToLight*0.5f*(fFOV/90.f) && fDistToLight<m_light.m_fRadius) && ((m_light.m_Flags & DLF_SUN) || GetCVars()->e_gsm_lods_num>1);

		if(bDoGSM)
		{
			Vec3 vSunDir = Get3DEngine()->GetSunDir().GetNormalized();		// todo: remove GetNormalized() once GetSunDir() returns the normalized value
			Vec3 vCameraDirWithoutDepth = vCameraDir - vCameraDir.Dot(vSunDir)*vSunDir;

			Vec3 vFocusPos = GetCamera().GetPosition() + vCameraDirWithoutDepth*fGSMBoxSize;
			SetBBox(AABB(vFocusPos-Vec3(fGSMBoxSize,fGSMBoxSize,fGSMBoxSize),vFocusPos+Vec3(fGSMBoxSize,fGSMBoxSize,fGSMBoxSize)));
		}
		else
			SetBBox(AABB(	m_light.m_Origin - Vec3(m_light.m_fRadius,m_light.m_fRadius,m_light.m_fRadius),
				m_light.m_Origin + Vec3(m_light.m_fRadius,m_light.m_fRadius,m_light.m_fRadius)));
 
    if(m_pShadowMapInfo->pGSM[nLod])
      m_pShadowMapInfo->pGSM[nLod]->bForSubSurfScattering = false;

		if(!ProcessFrustum(nLod, bDoGSM ? fGSMBoxSize : 0, fDistanceFromView, lstCastersHull))
		{
			nLod++;
			break;
		}

    if(m_pShadowMapInfo->pGSM[nLod])
    {
      m_pShadowMapInfo->pGSM[nLod]->bUseAdditiveBlending = false;
      m_pShadowMapInfo->pGSM[nLod]->bUseVarianceSM = false;
    }

		if(GetCVars()->e_shadows_from_terrain_in_all_lods && (nLod == GetCVars()->e_gsm_lods_num-1) && m_pShadowMapInfo->pGSM[nLod])
		{
			m_pShadowMapInfo->pGSM[nLod]->bUseAdditiveBlending = m_pShadowMapInfo->pGSM[nLod]->bUseVarianceSM = true;
		}

		if(GetCVars()->e_shadows_from_terrain_in_all_lods && nLod == GetCVars()->e_gsm_lods_num-2)
    {
      //should be at the end of frustums list
			fGSMBoxSize *= GetCVars()->e_gsm_range_step_terrain;
    }
		else
    {
      //near depth bound
      m_pShadowMapInfo->pGSM[nLod]->fMinFrustumBound = vEdgeScreen.y; //y is derection for view axis

      //compute plane  for next GSM slice
      vEdgeScreen = GSM_GetNextScreenEdge(fGSMBoxSize, fDistanceFromView);

      //far depth bound
      m_pShadowMapInfo->pGSM[nLod]->fMaxFrustumBound = vEdgeScreen.y;

			fGSMBoxSize *= GetCVars()->e_gsm_range_step;

      //compute distance from camera for next LOD
      fDistanceFromView = GSM_GetLODProjectionCenter(vEdgeScreen, fGSMBoxSize);
    }
	}

  //compute plane  for last GSM slice
  vEdgeScreen = GSM_GetNextScreenEdge(fGSMBoxSize, fDistanceFromView);
  float fShadowFadingDist = vEdgeScreen.GetLength() * 1.3;
  //update fading distance for LODs for current light source
  for(int j=0; j<GetCVars()->e_gsm_lods_num && j<MAX_GSM_LODS_NUM; j++)
  {
    if (m_pShadowMapInfo->pGSM[j])
    {
      //FIX: temporal hack for projector light sources
      if (m_light.m_Flags & DLF_PROJECT)
        m_pShadowMapInfo->pGSM[j]->fShadowFadingDist = 1024.0f;
      else
        m_pShadowMapInfo->pGSM[j]->fShadowFadingDist = fShadowFadingDist;

    }
  }




  //LOD for scattering rendering
  //that's all need to process ScatterLOD based on current position
  //TODO: insert gsm_cache and filtering for finding redernodes casters
  //and make orientation non-vertical
  if (/*GetCVars()->e_shadows_from_terrain_in_all_lods && */m_light.m_Flags & DLF_SUN && GetCVars()->e_gsm_lods_num<MAX_GSM_LODS_NUM)
  {
    Vec3 vSavedLightOrigin = m_light.m_Origin;
    m_light.m_Origin = Get3DEngine()->GetSunDirNormalized()*DISTANCE_TO_THE_SUN;//Get3DEngine()->m_vScatteringDir;
       
    //use LOD num which is just after last valid GSM lod
    int nScatterLOD = GetCVars()->e_gsm_lods_num;
		int nScatterRange;
		const AABB& bb = Get3DEngine()->m_BoundingBoxForScattering;
		if (bb.IsReset() || !GetRenderer()->IsMarkedForDepthMapCapture())
		{
			nScatterRange = (int)GetCVars()->e_gsm_scatter_lod_dist; 
			Vec3 vScatterDir = m_light.m_Origin.GetNormalized();//Get3DEngine()->m_vScatteringDir.GetNormalized();		// todo: remove GetNormalized() once GetSunDir() returns the normalized value
    Vec3 vCameraDirWithoutDepth = vCameraDir - vCameraDir.Dot(vScatterDir)*vScatterDir;
			Vec3 vFocusPos = GetCamera().GetPosition() + vCameraDirWithoutDepth * 0.7f * nScatterRange;
			SetBBox(AABB(vFocusPos-Vec3(nScatterRange,nScatterRange,nScatterRange),vFocusPos+Vec3(nScatterRange,nScatterRange,nScatterRange)));
		}
		else
		{
			nScatterRange = 1 + int(0.5f*(bb.max.x - bb.min.x));
			SetBBox(bb);
		}

    //TOFIX ShadowMapFrustum creation
    if(!(m_pShadowMapInfo->pGSM[nScatterLOD])) 
      m_pShadowMapInfo->pGSM[nScatterLOD] = new ShadowMapFrustum;
    m_pShadowMapInfo->pGSM[nScatterLOD]->bForSubSurfScattering = true;
    m_pShadowMapInfo->pGSM[nScatterLOD]->bUseAdditiveBlending = false;
    //m_pShadowMapInfo->pGSM[nScatterLOD]->nAffectsReceiversFrameId = 0;
    //  m_pShadowMapInfo->pGSM[nScatterLOD]->fMaxDistanceOfTheRecivers[0] = 0;
    ProcessFrustum(nScatterLOD, nScatterRange, 0, lstCastersHull); //ignore camera hull culling
//    m_pShadowMapInfo->pGSM[nScatterLOD]->nAffectsReceiversFrameId = GetMainFrameID();
//    m_pShadowMapInfo->pGSM[nScatterLOD]->fMaxDistanceOfTheRecivers[0] = DISTANCE_TO_THE_SUN*2;

    m_light.m_Origin = vSavedLightOrigin;

    //increase Gsm LOD number
    nLod++;
  }

	// free not used frustums
	for(; nLod<MAX_GSM_LODS_NUM; nLod++)
	{
		ShadowMapFrustum * & pFr = m_pShadowMapInfo->pGSM[nLod];
		if(pFr && pFr->pCastersList)
			pFr->pCastersList->Clear();
	}
}

bool CLightEntity::ProcessFrustum(int nLod, float fGSMBoxSize, float fDistanceFromView, PodArray<SPlaneObject> & lstCastersHull)
{
	ShadowMapFrustum * & pFr = m_pShadowMapInfo->pGSM[nLod];

	if(!pFr)
		pFr = new ShadowMapFrustum;

	// make shadow map frustum for receiving (include all objects into frustum)
	assert(pFr);

	//set zero all the reciver distances
/*	pFr->fMaxDistanceOfTheRecivers[0] =
		pFr->fMaxDistanceOfTheRecivers[1] =
		pFr->fMaxDistanceOfTheRecivers[2] =
		pFr->fMaxDistanceOfTheRecivers[3] =
		pFr->fMaxDistanceOfTheRecivers[4] =
		pFr->fMaxDistanceOfTheRecivers[5] = 0;
*/

	bool bDoGSM = fGSMBoxSize!=0;
	bool bDoNextLod = false;

	if(bDoGSM)// && (m_light.m_Flags & DLF_SUN || m_light.m_Flags & DLF_PROJECT))
	{
    if (
        (GetCVars()->e_shadows_from_terrain_in_all_lods && nLod < (GetCVars()->e_gsm_lods_num-1))  ||
        (!GetCVars()->e_shadows_from_terrain_in_all_lods && nLod < (GetCVars()->e_gsm_lods_num))
       )
		  InitShadowFrustum_SUN_Conserv(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST, fGSMBoxSize, fDistanceFromView, nLod);
    else
		  InitShadowFrustum_SUN(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST, fGSMBoxSize, nLod);
		FillFrustumCastersList_SUN(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST, lstCastersHull);
		bDoNextLod = true;
	}
	else if(m_light.m_Flags & DLF_PROJECT)
	{
		InitShadowFrustum_PROJECTOR(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST);
		FillFrustumCastersList_PROJECTOR(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST);
	}
	else
	{
		pFr->bOmniDirectionalShadow = true;
		InitShadowFrustum_OMNI(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST);
		FillFrustumCastersList_OMNI(pFr, SMC_EXTEND_FRUSTUM | SMC_SHADOW_FRUSTUM_TEST);
	}


  if(m_light.m_Flags & DLF_SUN)
  {
    float fTexRatio = (GetCVars()->e_shadows_max_texture_size / pFr->nTexSize);
    fTexRatio *= fTexRatio;

    float fVladRatio = min(fGSMBoxSize/3.f, 1.f);

    pFr->fDepthConstBias  = fVladRatio * (pFr->fFarDist - pFr->fNearDist) / (872727.27f * 2.0f)/** 0.000000001f;*/;
	  pFr->fDepthTestBias = fVladRatio * fTexRatio * (pFr->fFarDist - pFr->fNearDist) * (fGSMBoxSize*0.5f*0.5f + 0.5f) * 0.0000005f;
    pFr->fDepthSlopeBias = fVladRatio * GetCVars()->e_shadows_slope_bias * (fGSMBoxSize/GetCVars()->e_gsm_range) * 0.1;
  }
  else
  {
    float fTexRatio = (1.f/pFr->nTexSize);
    fTexRatio *= fTexRatio;

    pFr->fDepthConstBias  = 0.000003f * pFr->fFarDist; //should be reverted to 0.0000001 after fixinf +X-frustum
	  pFr->fDepthTestBias = 0.00028f * pFr->fFarDist;//(fTexRatio * 0.5) + */( (pFr->fFarDist - pFr->fNearDist) * 0.5f/1535.f );
    pFr->fDepthSlopeBias = GetCVars()->e_shadows_slope_bias;
  }

	if(pFr->fDepthTestBias>0.005f)
		pFr->fDepthTestBias=0.005f;

	if(pFr->fNearDist < 1000.f) // if not sun
		if(pFr->fDepthTestBias<0.0005f)
			pFr->fDepthTestBias=0.0005f;

	if(GetCVars()->e_shadows_frustums && pFr && pFr->pCastersList && pFr->pCastersList->Count())
		pFr->DrawFrustum(GetRenderer());


	return bDoNextLod;
}

// note: drop it
/*void GetPointBBoxDistances(const Vec3 & vPoint, const Vec3 & mins, const Vec3 & maxs,
float & fMinDist, float & fMaxDist)
{
Vec3 arrVerts3d[8] = 
{
Vec3(mins.x,mins.y,mins.z),
Vec3(mins.x,maxs.y,mins.z),
Vec3(maxs.x,mins.y,mins.z),
Vec3(maxs.x,maxs.y,mins.z),
Vec3(mins.x,mins.y,maxs.z),
Vec3(mins.x,maxs.y,maxs.z),
Vec3(maxs.x,mins.y,maxs.z),
Vec3(maxs.x,maxs.y,maxs.z)
};

typedef Triangle_tpl<float> MyTriangle;

//     1______3
//	  /      /|
//   /      / |
//	0------2  |
//	|   5  |  /7
//	|      | /
//	|      |/
//	4------6

float fDistancesSq[12] = 
{ 
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[0],arrVerts3d[1],arrVerts3d[2]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[1],arrVerts3d[3],arrVerts3d[2]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[5],arrVerts3d[6],arrVerts3d[7]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[5],arrVerts3d[4],arrVerts3d[6]))),

(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[0],arrVerts3d[2],arrVerts3d[6]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[0],arrVerts3d[6],arrVerts3d[4]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[2],arrVerts3d[3],arrVerts3d[7]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[2],arrVerts3d[7],arrVerts3d[6]))),

(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[5],arrVerts3d[1],arrVerts3d[0]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[5],arrVerts3d[0],arrVerts3d[4]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[3],arrVerts3d[1],arrVerts3d[5]))),
(Distance::Point_TriangleSq( vPoint, MyTriangle(arrVerts3d[3],arrVerts3d[5],arrVerts3d[7]))),
};

// take cry_sqrtf at end
fMinDist = fDistancesSq[0];
fMaxDist = fDistancesSq[0];
for(int i=0; i<12; i++)
{
if(fDistancesSq[i]<fMinDist)
fMinDist = fDistancesSq[i];
}

for(int i=0; i<8; i++)
{
float fVertDistSq = arrVerts3d[i].GetSquaredDistance(vPoint);
if(fVertDistSq>fMaxDist)
fMaxDist = fVertDistSq;
}

fMinDist = cry_sqrtf(fMinDist);
fMaxDist = cry_sqrtf(fMaxDist);
}*/


float frac_my(float fVal, float fSnap)
{
	float fValSnapped = fVal - fSnap*int(fVal/fSnap);
	return fValSnapped;
}

void CLightEntity::InitShadowFrustum_SUN(ShadowMapFrustum * pFr, int dwAllowedTypes, float fGSMBoxSize, int nLod)
{
	FUNCTION_PROFILER_3DENGINE;

  Vec3 vProjTranslation;
  if(GetCVars()->e_gsm_cache && !GetRenderer()->IsMarkedForDepthMapCapture())
    vProjTranslation = GetCamera().GetPosition();
  else
    vProjTranslation = GetBBox().GetCenter();

	if(!GetRenderer()->IsMarkedForDepthMapCapture() && !(GetRenderer()->IsMultiGPUModeActive()) && GetCVars()->e_gsm_cache && nLod>=GetCVars()->e_gsm_cache_lod_offset && pFr->fFOV && pFr->pDepthTex)
	{
		if (vProjTranslation.IsEquivalent(pFr->vProjTranslation, fGSMBoxSize*GetCVars()->e_gsm_cache))
		{
			int nFrameId = GetMainFrameID();
			int nLastFrameId = nFrameId-1;
			if((nFrameId & (1<<(nLod-GetCVars()->e_gsm_cache_lod_offset))) > (nLastFrameId & (1<<(nLod-GetCVars()->e_gsm_cache_lod_offset))))
				;
			else
				return;
		}
		//	DrawSphere(pFr->vProjTranslation, 2);
	}

  //update scattering map once per 50 frames
  if(!(GetRenderer()->IsMultiGPUModeActive()) && pFr->bForSubSurfScattering && pFr->fFOV && !GetRenderer()->IsMarkedForDepthMapCapture())
  {
    //TODO: should be added estimation based on max scattering area and current fGSMBoxSize
    if ( vProjTranslation.IsEquivalent(pFr->vProjTranslation, fGSMBoxSize/3) )
      return;
  }

  /*if(pFr->bUseAdditiveBlending && pFr->fFOV)
  {
    if ( vProjTranslation.IsEquivalent(pFr->vProjTranslation, fGSMBoxSize/40) )
      return;
    else
    {
      assert(0);
    }
  }*/

	pFr->RequestUpdate();
//  DetectCastersListChanges(pFr);

	pFr->nShadowMapLod=nLod;
	if (GetRenderer()->IsMarkedForDepthMapCapture() && pFr->bForSubSurfScattering)
		pFr->vLightSrcRelPos = m_light.m_Origin - GetBBox().GetCenter();
	else
	pFr->vLightSrcRelPos = m_light.m_Origin - GetCamera().GetPosition();
	pFr->fRadius = m_light.m_fRadius;
	assert(m_light.m_pOwner);
	pFr->pLightOwner = m_light.m_pOwner;
	pFr->m_Flags = m_light.m_Flags;

	const AABB & box = GetBBox();
	float fBoxRadius = box.GetRadius();

//	float fDistToLightSrc = pFr->vLightSrcRelPos.GetLength();
	pFr->fFOV = (float)RAD2DEG(cry_atanf((fBoxRadius + 0.25f)/DISTANCE_TO_THE_SUN))*1.9f;
	if(pFr->fFOV>160.f)
		pFr->fFOV=160.f;

  //Sampling parameters
  //Calculate proper projection of frustum to the terrain receiving area but not based on fBoxRadius
  //TOFIX = recalculate based on e_gsm_range and step
  float fBoxRadiusMax = 76.043961f;
  float arrBlurS[] = {-17, -1, 0, 1, 0, 0, 0, 0};
  pFr->fWidthS = /*GetCVars()->e_shadows_max_texture_size * */fBoxRadiusMax/fBoxRadius  ;
  pFr->fWidthT = pFr->fWidthS;
  pFr->fBlurS = arrBlurS[nLod];
  pFr->fBlurT = pFr->fBlurS;

	Vec3 vLightDir = pFr->vLightSrcRelPos.normalized();

	pFr->fCameraFarPlaneGSM = fBoxRadius;

	// Hack, to prevent the clipping of shadow/scatter casting objects.
	float fScatterNearDistMultiplier = 1.0f;
	if (pFr->bForSubSurfScattering) 
		fScatterNearDistMultiplier = 5.0f;

	float fDist = pFr->vLightSrcRelPos.GetLength();
	if((pFr->bUseVarianceSM && pFr->bUseAdditiveBlending) || (GetRenderer()->IsMarkedForDepthMapCapture() && pFr->bForSubSurfScattering))
	{
		pFr->fNearDist = (fDist - fGSMBoxSize*3.f * fScatterNearDistMultiplier);
		pFr->fFarDist  = (fDist + fGSMBoxSize*2.f);
	}
	else
	{
		pFr->fNearDist = (fDist - GetCVars()->e_sun_clipplane_range * fScatterNearDistMultiplier);
		pFr->fFarDist  = (fDist + GetCVars()->e_sun_clipplane_range);
	}

	if(pFr->fFarDist>m_light.m_fRadius)
		pFr->fFarDist=m_light.m_fRadius;

	if(pFr->fNearDist<pFr->fFarDist*0.005f)
		pFr->fNearDist=pFr->fFarDist*0.005f;

	assert(pFr->fNearDist < pFr->fFarDist);

	if (GetRenderer()->IsMarkedForDepthMapCapture())
		pFr->nTexSize = TEXTURE_SIZE_FOR_VERTEX_SCATTERING_CAPTURE;
	else
	pFr->nTexSize = GetCVars()->e_shadows_max_texture_size;

	if(pFr->isUpdateRequested(0))
		pFr->vProjTranslation = vProjTranslation;

	// local jitter amount depends on frustum size
	pFr->fFrustrumSize =  1.0f / (fGSMBoxSize*(float)GetCVars()->e_gsm_range);
	pFr->nUpdateFrameId = GetFrameID();
	pFr->bAllowViewDependency = GetCVars()->e_gsm_view_space != 0;

  //Get gsm bounds
  GetGsmFrustumBounds(GetCamera(), pFr);

	//	pFr->fDepthTestBias = (pFr->fFarDist - pFr->fNearDist) * 0.0001f * (fGSMBoxSize/2.f);
}

void CLightEntity::InitShadowFrustum_SUN_Conserv(ShadowMapFrustum * pFr, int dwAllowedTypes, float fGSMBoxSize, float fDistance, int nLod)
{
  FUNCTION_PROFILER_3DENGINE;

  //TOFIX: replace fGSMBoxSize  by fRadius
  float fRadius = fGSMBoxSize;


  //Calc center of projection based on the current screen and LOD radius
  //Vec3 vEdgeScreen = GetCamera().GetEdgeN(); //GetEdgeP();
  Vec3 vViewDir = GetCamera().GetViewdir();
  //float fDistance = cry_sqrtf( fGSMBoxSize*fGSMBoxSize - vEdgeScreen.z*vEdgeScreen.z - vEdgeScreen.x*vEdgeScreen.x ) + vEdgeScreen.y;

  Vec3 vProjTranslation = GetCamera().GetPosition() + fDistance * vViewDir;

  //DrawSphere(vProjTranslation, fGSMBoxSize/20);
  if(!(GetRenderer()->IsMultiGPUModeActive()) && GetCVars()->e_gsm_cache && nLod>=GetCVars()->e_gsm_cache_lod_offset && pFr->fFOV)
  {
    if (vProjTranslation.IsEquivalent(pFr->vProjTranslation, fGSMBoxSize*GetCVars()->e_gsm_cache))
    {
      int nFrameId = GetMainFrameID();
      int nLastFrameId = nFrameId-1;
      if((nFrameId & (1<<(nLod-GetCVars()->e_gsm_cache_lod_offset))) > (nLastFrameId & (1<<(nLod-GetCVars()->e_gsm_cache_lod_offset))))
        ;
      else
        return;
    }
    //	DrawSphere(pFr->vProjTranslation, 2);
  }

  assert (! (pFr->bForSubSurfScattering) );
  //update scattering map onece per 50 frames
  if(!(GetRenderer()->IsMultiGPUModeActive()) && pFr->bForSubSurfScattering)
  {
    //TODO: should be added estimation based on max scattering area and current fGSMBoxSize
    if ( vProjTranslation.IsEquivalent(pFr->vProjTranslation, fGSMBoxSize/3) )
      return;
  }

  pFr->RequestUpdate();

  pFr->nShadowMapLod=nLod;
  pFr->vLightSrcRelPos = m_light.m_Origin - GetCamera().GetPosition();
  pFr->fRadius = m_light.m_fRadius;
  assert(m_light.m_pOwner);
  pFr->pLightOwner = m_light.m_pOwner;
  pFr->m_Flags = m_light.m_Flags;

  const AABB & box = GetBBox();
  float fBoxRadius = box.GetRadius();

  //	float fDistToLightSrc = pFr->vLightSrcRelPos.GetLength();
  pFr->fFOV = (float)RAD2DEG(cry_atanf(fRadius/DISTANCE_TO_THE_SUN)) * 2.0f;
  if(pFr->fFOV>160.f)
    pFr->fFOV=160.f;

  //Sampling parameters
  //Calculate proper projection of frustum to the terrain receiving area but not based on fBoxRadius
  float fBoxRadiusMax = 140.29611f;
  float arrBlurS[] = {-25, -8, -2.3f, -0.5f, 0, 0, 0, 0};
  pFr->fWidthS = /*GetCVars()->e_shadows_max_texture_size * */fBoxRadiusMax/fBoxRadius  ;
  pFr->fWidthT = pFr->fWidthS;
  pFr->fBlurS = arrBlurS[nLod];
  pFr->fBlurT = pFr->fBlurS;

  Vec3 vLightDir = pFr->vLightSrcRelPos.normalized();

  pFr->fCameraFarPlaneGSM = fBoxRadius;

  float fDist = pFr->vLightSrcRelPos.GetLength();
  /*if(fGSMBoxSize>500)
  {
    pFr->fNearDist = (fDist - fGSMBoxSize*2.f);
    pFr->fFarDist  = (fDist + fGSMBoxSize*2.f);
  }
  else*/
  {
    pFr->fNearDist = (fDist - GetCVars()->e_sun_clipplane_range);
    pFr->fFarDist  = (fDist + GetCVars()->e_sun_clipplane_range);
  }

  if(pFr->fFarDist>m_light.m_fRadius)
    pFr->fFarDist=m_light.m_fRadius;

  if(pFr->fNearDist<pFr->fFarDist*0.005f)
    pFr->fNearDist=pFr->fFarDist*0.005f;

  assert(pFr->fNearDist < pFr->fFarDist);

  pFr->nTexSize = GetCVars()->e_shadows_max_texture_size;

  if(pFr->isUpdateRequested(0))
    pFr->vProjTranslation = vProjTranslation;

  // local jitter amount depends on frustum size
  pFr->fFrustrumSize =  1.0f / (fGSMBoxSize*(float)GetCVars()->e_gsm_range);
  pFr->nUpdateFrameId = GetFrameID();
  pFr->bAllowViewDependency = GetCVars()->e_gsm_view_space != 0;

  //Get gsm bounds
  GetGsmFrustumBounds(GetCamera(), pFr);

  //	pFr->fDepthTestBias = (pFr->fFarDist - pFr->fNearDist) * 0.0001f * (fGSMBoxSize/2.f);
}


bool SegmentFrustumIntersection(const Vec3& P0,const Vec3& P1, const CCamera& frustum, Vec3* pvIntesectP0 = NULL, Vec3* pvIntesectP1 = NULL)
{
	if (P0.IsEquivalent(P1))
	{
		return frustum.IsPointVisible(P0);
	}

	//Actual Segment-Frustum intersection test
	float tE = 0.0f;
	float tL = 1.0f;
	Vec3 dS = P1 - P0;

	for (int i=0; i<6; i++)
	{
		const Plane* currPlane = frustum.GetFrustumPlane(i);

		Vec3 ni = currPlane->n;
		Vec3 Vi = ni * (-currPlane->d);

		float N = -( ni | Vec3(P0 - Vi));
		float D = ni | dS;

		if (D == 0) //segment is parallel to face
		{
			if (N < 0) 
				return false; //outside face
			else
				continue; //inside face
		}

		float t = N / D;
		if (D < 0) //segment is entering face
		{
			tE = max(tE,t);
			if (tE > tL)
					return false;
		}
		else //segment is leaving face
		{
			tL = min(tL,t);
			if (tL < tE)
					return false;
		}
	}
  //calc intersection point if needed
  if (pvIntesectP0)
    *pvIntesectP0 = P0 + tE * dS;   // = P(tE) = point where S enters polygon
  if (pvIntesectP1)
    *pvIntesectP1 = P0 + tL * dS;   // = P(tL) = point where S leaves polygon

	//it's intersecting frustum 
	return true;
}


bool FrustumIntersection(const CCamera& viewFrustum, const CCamera& shadowFrustum)
{
	int i;

	Vec3 pvViewFrust[8];
	Vec3 pvShadowFrust[8];

	viewFrustum.GetFrustumVertices(pvViewFrust);
	shadowFrustum.GetFrustumVertices(pvShadowFrust);


	//test points inclusion
	//8 points
	for(i=0; i<8; i++)
	{
		if (viewFrustum.IsPointVisible(pvShadowFrust[i]))
		{
			return true;
		}

		if (shadowFrustum.IsPointVisible(pvViewFrust[i]))
		{
			return true;
		}
	}

	//fixme: clean code with frustum edges
	//12 edges
	for (i=0; i<4; i++)
	{
		//far face
		if( SegmentFrustumIntersection(pvShadowFrust[i], pvShadowFrust[(i+1)%4],viewFrustum) )
			return true;
		//near face
		if( SegmentFrustumIntersection(pvShadowFrust[i + 4], pvShadowFrust[(i+1)%4 + 4],viewFrustum) )
			return true;
		//other edges
		if( SegmentFrustumIntersection(pvShadowFrust[i], pvShadowFrust[i+4], viewFrustum) )
			return true;

		//vice-versa test
		//far face
		if( SegmentFrustumIntersection(pvViewFrust[i], pvViewFrust[(i+1)%4],shadowFrustum) )
			return true;
		//near face
		if( SegmentFrustumIntersection(pvViewFrust[i + 4], pvViewFrust[(i+1)%4 + 4],shadowFrustum) )
			return true;
		//other edges
		if( SegmentFrustumIntersection(pvViewFrust[i], pvViewFrust[i+4], shadowFrustum) )
			return true;
	}

	return false;
}

//TODO: should be in the renderer or in 3dengine only (with FrustumIntersection)
/*void GetShadowMatrixOrtho(Matrix44& mLightProj, Matrix44& mLightView, const Matrix44& mViewMatrix, ShadowMapFrustum* lof, bool bViewDependent)
{

  mathMatrixPerspectiveFov(&mLightProj, lof->fFOV*(gf_PI/180.0f), lof->fProjRatio, lof->fNearDist, lof->fFarDist);

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

  //D3DXVECTOR3 eyeLightDir;
  //D3DXVec3TransformNormal(&eyeLightDir, &vLightDir, pmViewMatrix);

  if (bViewDependent)
  {
    //we should have LightDir vector transformed to the view space

    Eye = GetTransposed44(mViewMatrix).TransformPoint(Eye);
    At = GetTransposed44(mViewMatrix).TransformPoint(At);

    vLightDir = GetTransposed44(mViewMatrix).TransformVector(vLightDir);
    //vLightDir.Normalize();
  }

  if ( fabsf(vLightDir.Dot(zAxis))>0.95f )
    Up = yAxis;
  else
    Up = zAxis;

  //Eye	= Vec3(8745.8809, 1281.8682, 5086.0918);
  //At = Vec3(212.88541, 90.157082, 8.6914768);
  //Up = Vec3(0.00000000, 0.00000000, 1.0000000);

  //get look-at matrix
  mathMatrixLookAt(&mLightView, Eye, At, Up);

  //we should transform coords to the view space, so shadows are oriented according to camera always
  if (bViewDependent)
  {
    mLightView = mViewMatrix * mLightView;
  }

}*/

bool CLightEntity::GetGsmFrustumBounds(const CCamera& viewFrustum, ShadowMapFrustum* pShadowFrustum)
{
  int i;

  Vec3 pvViewFrust[8];
  Vec3 pvShadowFrust[8];

  CCamera& camShadowFrustum = pShadowFrustum->FrustumPlanes;

  Matrix34 mShadowView, mShadowProj;
  Matrix34 mShadowComposite;

  //get composite shadow matrix to compute bounds
  //FIX: check viewFrustum.GetMatrix().GetInverted()???
	Matrix44 mCameraView = viewFrustum.GetMatrix().GetInverted();
  /*GetShadowMatrixOrtho( mShadowProj,
                        mShadowView,
                        mCameraView, pShadowFrustum, false);*/

  //Pre-multiply matrices
  //renderer coord system
  //FIX: replace this hack with full projective depth bound computations (light space z currently)
  mShadowComposite =  viewFrustum.GetMatrix().GetInverted(); /** mShadowProj*/;
  //GetTransposed44(Matrix44(Matrix33::CreateRotationX(-gf_PI/2) * viewFrustum.GetMatrix().GetInverted()));

  //set CCamera
  //engine coord system
  //FIX: should be processed like this
  //camShadowFrustum.SetMatrix( Matrix34( GetTransposed44(mShadowView) ) );
  //camShadowFrustum.SetFrustum(pShadowFrustum->nTexSize,pShadowFrustum->nTexSize,pShadowFrustum->fFOV*(gf_PI/180.0f), pShadowFrustum->fNearDist, pShadowFrustum->fFarDist);

  viewFrustum.GetFrustumVertices(pvViewFrust);
  camShadowFrustum.GetFrustumVertices(pvShadowFrust);

  Vec3 vCamPosition = viewFrustum.GetPosition();

  Vec3 vIntersectP0(0,0,0), vIntersectP1(0,0,0);

  AABB viewAABB;
  viewAABB.Reset();

  bool bIntersected = false;
  Vec3 vP0_NDC, vP1_NDC;
  f32 fZ_NDC_Min = 2048.0f; // far plane
  f32 fZ_NDC_Max = 0.0f; // nead plane
  f32 fDisatanceToMaxBound = 0;
  Vec3 vMaxBoundPoint;

  //fixme: clean code with frustum edges
  //12 edges
  for (i=0; i<4; i++)
  {
    //far face
    if( SegmentFrustumIntersection( pvShadowFrust[i], pvShadowFrust[(i+1)%4],viewFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);

      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }

    //near face
    if( SegmentFrustumIntersection(pvShadowFrust[i + 4], pvShadowFrust[(i+1)%4 + 4],viewFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);


      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }


    //other edges
    if( SegmentFrustumIntersection(pvShadowFrust[i], pvShadowFrust[i+4], viewFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);

      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }


    //vice-versa test
    //far face
    if( SegmentFrustumIntersection(pvViewFrust[i], pvViewFrust[(i+1)%4],camShadowFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);

      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }



    if( SegmentFrustumIntersection(pvViewFrust[i + 4], pvViewFrust[(i+1)%4 + 4],camShadowFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);

      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }



    //other edges
    if( SegmentFrustumIntersection(pvViewFrust[i], pvViewFrust[i+4], camShadowFrustum, &vIntersectP0, &vIntersectP1 ))
    {
      //viewAABB.Add(vIntersectP0);
      //viewAABB.Add(vIntersectP1);

      if (GetCVars()->e_gsm_depth_bounds_debug)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawLine(vIntersectP0, RGBA8(0xff,0xff,0x1f,0xff), vIntersectP1, RGBA8(0xff,0xff,0x1f,0xff), 2.0f );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP0, RGBA8(0xff,0xff,0xff,0xff), 10 );
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vIntersectP1, RGBA8(0xff,0xff,0xff,0xff), 10 );
      }

      //todo: move to func
      //projection
      //vP0_NDC = Vec4(vIntersectP0,1.0f) * mShadowComposite;
      //vP0_NDC /= vP0_NDC.w;

      /*vP0_NDC = mShadowComposite.TransformPoint(vIntersectP0);

      fZ_NDC_Min = min(fZ_NDC_Min, vP0_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP0_NDC.z);*/

      //vP1_NDC = Vec4(vIntersectP1,1.0f) * mShadowComposite;
      //vP1_NDC /= vP1_NDC.w;

      /*vP1_NDC = mShadowComposite.TransformPoint(vIntersectP1);

      fZ_NDC_Min = min(fZ_NDC_Min, vP1_NDC.z);
      fZ_NDC_Max = max(fZ_NDC_Max, vP1_NDC.z);*/

      float fCurDistance = (vCamPosition - vIntersectP0).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP0;
        fDisatanceToMaxBound = fCurDistance;
      }

      fCurDistance = (vCamPosition - vIntersectP1).GetLength();
      if (fCurDistance>fDisatanceToMaxBound)
      {
        vMaxBoundPoint = vIntersectP1;
        fDisatanceToMaxBound = fCurDistance;
      }

      bIntersected = true;
    }

  }

  if (GetCVars()->e_gsm_depth_bounds_debug)
  {
    GetRenderer()->GetIRenderAuxGeom()->DrawPoint(vMaxBoundPoint, RGBA8(0xff,0x00,0x00,0xff), 10 );
  }

  /*Matrix44 mLightProj;
  mathMatrixPerspectiveFov(&mLightProj, viewFrustum.GetFov(), viewFrustum.GetProjRatio(), viewFrustum.GetNearPlane(), viewFrustum.GetFarPlane());

  Vec4 vMaxDir = Vec4(0, 0, -fZ_NDC_Max, 1) * mLightProj;
  vMaxDir /= vMaxDir.w;*/
 
  pShadowFrustum->fMinFrustumBound = 0/*fZ_NDC_Min*/;
  pShadowFrustum->fMaxFrustumBound = fZ_NDC_Max; //vMaxDir.z;
  pShadowFrustum->vMaxBoundPoint = vMaxBoundPoint;


  //Get3DEngine()->DrawBBox(viewAABB);

  return bIntersected ;
}


void GetCubemapFrustum(ShadowMapFrustum * pFr, int nS, CCamera& shadowFrust)
{
	//FIX: don't generate frustum in renderer
	float sCubeVector[6][7] = 
	{
		{1,0,0,  0,0,1, -90}, //posx
		{-1,0,0, 0,0,1,  90}, //negx
		{0,1,0,  0,0,-1, 0},  //posy
		{0,-1,0, 0,0,1,  0},  //negy
		{0,0,1,  0,1,0,  0},  //posz
		{0,0,-1, 0,1,0,  0},  //negz 
	};

	int nShadowTexSize = pFr->nTexSize;
	Vec3 vPos = pFr->vLightSrcRelPos + pFr->vProjTranslation;

	Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
	Vec3 vUp      = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
	Matrix33 matRot = Matrix33::CreateOrientation(vForward, vUp, DEG2RAD(sCubeVector[nS][6]));

	float fMinDist = pFr->fNearDist;
	float fMaxDist = pFr->fFarDist;
	shadowFrust.SetMatrix(Matrix34(matRot, vPos));
	shadowFrust.SetFrustum(nShadowTexSize, nShadowTexSize, 90.0f*gf_PI/180.0f, fMinDist, fMaxDist);
}

void CLightEntity::CheckValidFrustums_OMNI(ShadowMapFrustum * pFr)
{
	pFr->nOmniFrustumMask = 0;

	const CCamera& cameraFrust = GetCamera();

	for (int nS=0; nS<6; nS++)
	{

		CCamera shadowFrust;
		GetCubemapFrustum(pFr, nS, shadowFrust);

		if( FrustumIntersection(cameraFrust, shadowFrust) )
			pFr->nOmniFrustumMask |= (1 << nS);

	}
}

bool CLightEntity::CheckFrustumsIntersect(CLightEntity* lightEnt)
{
	Vec3 pvShadowFrust[8];
	bool bRes = false;

	ShadowMapFrustum* pFr1;
	ShadowMapFrustum* pFr2;

	pFr1 = this->GetShadowFrustum(0);
	pFr2 = lightEnt->GetShadowFrustum(0);

	if (!pFr1 || !pFr2)
		return false;

	int nFaces1, nFaces2;
	nFaces1 = (pFr1->bOmniDirectionalShadow?6:1);
	nFaces2 = (pFr2->bOmniDirectionalShadow?6:1);

	for (int nS1=0; nS1<nFaces1; nS1++)
	{
		for (int nS2=0; nS2<nFaces2; nS2++)
		{
			CCamera shadowFrust1, shadowFrust2;

			if (pFr1->bOmniDirectionalShadow) 
				GetCubemapFrustum(pFr1, nS1, shadowFrust1);
			else
				shadowFrust1 = pFr1->FrustumPlanes;

			if (pFr2->bOmniDirectionalShadow) 
				GetCubemapFrustum(pFr2, nS2, shadowFrust2);
			else
				shadowFrust2 = pFr2->FrustumPlanes;

			if( FrustumIntersection(shadowFrust1, shadowFrust2) )
			{
				bRes = true;

				//debug frustums
					uint16 pnInd[8] = {
						0,4,1,5,2,6,3,7
					};
					//first frustum
					shadowFrust1.GetFrustumVertices(pvShadowFrust);
					GetRenderer()->GetIRenderAuxGeom()->DrawPolyline(pvShadowFrust, 4, true, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawPolyline(pvShadowFrust+4, 4, true, RGBA8(0xff,0xff,0x1f,0xff));

					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+2, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+4, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+6, 2, RGBA8(0xff,0xff,0x1f,0xff));

					//second frustum
					shadowFrust2.GetFrustumVertices(pvShadowFrust);
					GetRenderer()->GetIRenderAuxGeom()->DrawPolyline(pvShadowFrust, 4, true, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawPolyline(pvShadowFrust+4, 4, true, RGBA8(0xff,0xff,0x1f,0xff));

					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+2, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+4, 2, RGBA8(0xff,0xff,0x1f,0xff));
					GetRenderer()->GetIRenderAuxGeom()->DrawLines(pvShadowFrust, 8, pnInd+6, 2, RGBA8(0xff,0xff,0x1f,0xff));

			}
		}
	}
 return bRes;
}

void CLightEntity::InitShadowFrustum_PROJECTOR(ShadowMapFrustum * pFr, int dwAllowedTypes)
{
	FUNCTION_PROFILER_3DENGINE;

	// construct camera from projector
	Matrix34 entMat = ((ILightSource*)m_light.m_pOwner)->GetMatrix();

	Vec3 vProjDir = entMat.GetColumn(0).GetNormalizedSafe();

	pFr->nShadowMapLod=-1;		// not used

	// place center into middle of projector far plane 
	pFr->vLightSrcRelPos = - vProjDir*m_light.m_fRadius;
	pFr->vProjTranslation = m_light.m_Origin - pFr->vLightSrcRelPos;
	if(pFr->fRadius != m_light.m_fRadius)
    pFr->RequestUpdate();
  pFr->fRadius = m_light.m_fRadius;
	assert(m_light.m_pOwner && m_light.m_pOwner==this);
	pFr->pLightOwner = this;
	pFr->m_Flags = m_light.m_Flags;

	pFr->fCameraFarPlaneGSM = m_light.m_fRadius;
//	float fDistToLightSrc = pFr->vLightSrcRelPos.GetLength();
	pFr->fFOV = CLAMP(m_light.m_fLightFrustumAngle*2.f, 1.f, 120.f);

	pFr->fNearDist = 0.01f;
	pFr->fFarDist = m_light.m_fRadius;

	// set texture size
	int nTexSize = GetCVars()->e_shadows_max_texture_size;
	if(pFr->bOmniDirectionalShadow)
		nTexSize = GetCVars()->e_shadows_max_texture_size/2;

	float fLightToCameraDist = max(5.f, GetCamera().GetPosition().GetDistance(m_light.m_Origin) - m_light.m_fRadius);
	while(nTexSize > (800.f/fLightToCameraDist)*pFr->fRadius*m_light.m_Color.Luminance()*(pFr->fFOV/90.f) && nTexSize>256)
		nTexSize /= 2;

	if(pFr->nTexSize != nTexSize)
	{
		pFr->nTexSize = nTexSize;
		pFr->RequestUpdate();
	}

	//	m_pShadowMapInfo->bUpdateRequested = false;
	//	pFr->bUpdateRequested = true;
	// local jitter amount depends on frustum size
	//FIX:: non-linear adjustment for fFrustrumSize
	pFr->fFrustrumSize =  20.0f * nTexSize/64.0f; // / (fGSMBoxSize*(float)GetCVars()->e_gsm_range);
	//	pFr->nDLightId = m_light.m_Id;
	pFr->nUpdateFrameId = GetFrameID();

	//	float fGSMBoxSize =  pFr->fRadius * cry_tanf(DEG2RAD(pFr->fFOV)*0.5f);

	//	pFr->fDepthTestBias = (pFr->fFarDist - pFr->fNearDist) * 0.0001f * (fGSMBoxSize/2.f);
	//	pFr->fDepthTestBias *= (pFr->fFarDist - pFr->fNearDist) / 256.f;
}

void CLightEntity::InitShadowFrustum_OMNI(ShadowMapFrustum * pFr, int dwAllowedTypes)
{
	InitShadowFrustum_PROJECTOR(pFr, dwAllowedTypes);
	CheckValidFrustums_OMNI(pFr);
}

bool IsABBBVisibleInFrontOfPlane_FAST(const AABB & objBox, const SPlaneObject & clipPlane);

bool IsAABBInsideHull(SPlaneObject * pHullPlanes, int nPlanesNum, const AABB & aabbBox)
{
	for(int i=0; i<nPlanesNum; i++)
	{
		if(!IsABBBVisibleInFrontOfPlane_FAST(aabbBox,pHullPlanes[i]))
			return false;
	}

	return true;
}

int CLightEntity::MakeShadowCastersHull(PodArray<SPlaneObject> & lstCastersHull)
{
	FUNCTION_PROFILER_3DENGINE;

	Vec3 vCamPos = GetCamera().GetPosition();

	// construct hull from camera vertices and light source position
	Vec3 arrFrustVerts[16];
	memset(arrFrustVerts,0,sizeof(arrFrustVerts));
	GetCamera().GetFrustumVertices(arrFrustVerts);

	for(int v=0; v<4; v++)
		arrFrustVerts[v] = vCamPos + (arrFrustVerts[v] - vCamPos).normalized()*GetObjManager()->m_fGSMMaxDistance*1.3f; //0.2f;
	arrFrustVerts[4] = GetCamera().GetPosition();

	Vec3 vSunDir = (m_light.m_Origin - vCamPos).normalized()*GetCVars()->e_sun_clipplane_range;
	for(int v=0; v<5; v++)
		arrFrustVerts[v+5] = arrFrustVerts[v] + vSunDir;

	// make array of unique planes

	static index_t indsBuf[256],*pInds=indsBuf;
	int nPlanesNumWithDuplicates = GetPhysicalWorld()->GetPhysUtils()->qhull(&arrFrustVerts[0], 10, pInds);

	for(int t=0; t<nPlanesNumWithDuplicates; t++)
	{
		Plane newPlane = 
			Plane::CreatePlane(arrFrustVerts[pInds[t*3+0]], arrFrustVerts[pInds[t*3+2]], arrFrustVerts[pInds[t*3+1]]);

		int j=0;
		for(j=0; j<lstCastersHull.Count(); j++)
		{
			if(IsEquivalent(newPlane.n,lstCastersHull[j].plane.n))
				if(fabs(newPlane.d-lstCastersHull[j].plane.d)<0.01f)
					break;
		}

		if(j==lstCastersHull.Count())
		{
			SPlaneObject po;
			po.plane = newPlane;
			po.Update();
			lstCastersHull.Add(po);
		}
	}

	return lstCastersHull.Count();
}

void CLightEntity::FillFrustumCastersList_SUN(ShadowMapFrustum * pFr, int dwAllowedTypes, PodArray<SPlaneObject> & lstCastersHull)
{
	FUNCTION_PROFILER_3DENGINE;

	pFr->bOmniDirectionalShadow = false;

	Vec3 vMapCenter = Vec3(CTerrain::GetTerrainSize()*0.5f, CTerrain::GetTerrainSize()*0.5f, CTerrain::GetTerrainSize()*0.25f);

	// prevent crash in qhull
	if(!dwAllowedTypes || !((GetCamera().GetPosition() - vMapCenter).GetLength()<CTerrain::GetTerrainSize()*4))
		return;

	// setup camera

	CCamera & FrustCam = pFr->FrustumPlanes = CCamera();
	Vec3 vLightDir = -pFr->vLightSrcRelPos.normalized();

	Matrix34 mat;

	if (GetCVars()->e_gsm_view_space > 0)
	{
		Matrix44	matView = GetCamera().GetMatrix().GetInverted();
		Vec3 vEyeLightDir = matView.TransformVector(vLightDir);
		mat = Matrix33::CreateRotationVDir( vEyeLightDir );
		mat.SetTranslation(matView.TransformPoint(pFr->vLightSrcRelPos + pFr->vLightSrcRelPos ));
		mat = GetCamera().GetMatrix() * mat; 
	}
	else
	{
		mat = Matrix33::CreateRotationVDir( vLightDir );
		mat.SetTranslation(pFr->vLightSrcRelPos + pFr->vProjTranslation);
	}

	FrustCam.SetMatrix(mat);
	FrustCam.SetFrustum(256,256,pFr->fFOV*(gf_PI/180.0f), pFr->fNearDist, pFr->fFarDist);

	if (!pFr->bForSubSurfScattering)
	{
	if(!lstCastersHull.Count()) // make hull first time it is needed
		MakeShadowCastersHull(lstCastersHull);
	}

	//  fill casters list
	if(!pFr->pCastersList)
		pFr->pCastersList = new PodArray<IShadowCaster*>;

	if(pFr->isUpdateRequested(0))
  {
    if (pFr->bForSubSurfScattering) // ignore camera hull culling 
    {
		  m_pObjManager->MakeShadowCastersList((CVisArea*)GetEntityVisArea(), GetBBox(), 
  			dwAllowedTypes, pFr->vLightSrcRelPos + pFr->vLightSrcRelPos, &m_light, pFr, NULL );
    }
    else
    {
      m_pObjManager->MakeShadowCastersList((CVisArea*)GetEntityVisArea(), GetBBox(), 
        dwAllowedTypes, pFr->vLightSrcRelPos + pFr->vLightSrcRelPos, &m_light, pFr, &lstCastersHull );
    }
  }
/*
  if(GetCVars()->e_gsm_cache)
  {
    pFr->nInvalidatedFrustMask = 0;
    DetectCastersListChanges(pFr);
  }*/
}

void CLightEntity::FillFrustumCastersList_PROJECTOR(ShadowMapFrustum * pFr, int dwAllowedTypes)
{
	FUNCTION_PROFILER_3DENGINE;

	//  fill casters list
	if(pFr->pCastersList)
		pFr->pCastersList->Clear();
	else
		pFr->pCastersList = new PodArray<IShadowCaster*>;

	pFr->bOmniDirectionalShadow = false;

	if(dwAllowedTypes)
	{
		// setup camera
		CCamera & FrustCam = pFr->FrustumPlanes = CCamera();
		Vec3 vLightDir = -pFr->vLightSrcRelPos.normalized();
		Matrix34 mat = Matrix33::CreateRotationVDir( vLightDir );
		mat.SetTranslation(GetBBox().GetCenter());

		FrustCam.SetMatrix(mat);
		FrustCam.SetFrustum(pFr->nTexSize,pFr->nTexSize,pFr->fFOV*(gf_PI/180.0f), pFr->fNearDist, pFr->fFarDist);


		m_pObjManager->MakeShadowCastersList((CVisArea*)GetEntityVisArea(), GetBBox(), 
			dwAllowedTypes, pFr->vLightSrcRelPos + GetBBox().GetCenter(), &m_light, pFr, NULL );

		DetectCastersListChanges(pFr);

		pFr->aabbCasters.Reset(); // fix: should i .Reset() pFr->aabbCasters ?
	}
}

void CLightEntity::FillFrustumCastersList_OMNI(ShadowMapFrustum * pFr, int dwAllowedTypes)
{
	FUNCTION_PROFILER_3DENGINE;

	//  fill casters list
	if(pFr->pCastersList)
		pFr->pCastersList->Clear();
	else
		pFr->pCastersList = new PodArray<IShadowCaster*>;

	if(dwAllowedTypes)
	{
		// setup camera
		CCamera & FrustCam = pFr->FrustumPlanes = CCamera();
		Vec3 vLightDir = -pFr->vLightSrcRelPos.normalized();
		Matrix34 mat = Matrix33::CreateRotationVDir( vLightDir );
		mat.SetTranslation(GetBBox().GetCenter());

		FrustCam.SetMatrix(mat);
		FrustCam.SetFrustum(256,256,pFr->fFOV*(gf_PI/180.0f)*0.9f, pFr->fNearDist, pFr->fFarDist);

		m_pObjManager->MakeShadowCastersList((CVisArea*)GetEntityVisArea(), GetBBox(), 
			dwAllowedTypes, pFr->vLightSrcRelPos + GetBBox().GetCenter(), &m_light, pFr, NULL );

		DetectCastersListChanges(pFr);

		pFr->aabbCasters.Reset(); // fix: should i .Reset() pFr->aabbCasters ?
	}
}

void CLightEntity::DetectCastersListChanges(ShadowMapFrustum * pFr)
{
	uint uCastersListCheckSum = 0;
	if(pFr->pCastersList)
		for(int i=0; i<pFr->pCastersList->Count(); i++)
		{
			IShadowCaster * pNode = pFr->pCastersList->GetAt(i);
			const AABB entBox = pNode->GetBBoxVirtual();
			uCastersListCheckSum += uint((entBox.min.x + entBox.min.y + entBox.min.z)*10000.f);
			uCastersListCheckSum += uint((entBox.max.x + entBox.max.y + entBox.max.z)*10000.f);

      ICharacterInstance* pChar = pNode->GetEntityCharacter(0);

      if (pChar)
      {
        ISkeletonAnim* pISkeletonAnim = pChar->GetISkeletonAnim();
        if (pISkeletonAnim) 
        {
          uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0) ;
          if (numAnimsLayer0!=0) 
          {
            pFr->RequestUpdate();
          }
        }
      }
		}

    if(pFr->fRadius<DISTANCE_TO_THE_SUN)
    {
		  uCastersListCheckSum += uint((m_WSBBox.min.x + m_WSBBox.min.y + m_WSBBox.min.z)*10000.f);
		  uCastersListCheckSum += uint((m_WSBBox.max.x + m_WSBBox.max.y + m_WSBBox.max.z)*10000.f);
    }

		if(pFr->uCastersListCheckSum != uCastersListCheckSum)
		{
			pFr->RequestUpdate();
			pFr->uCastersListCheckSum = uCastersListCheckSum;

			if(GetCVars()->e_shadows_debug == 3)
			{
				const char * szName = ((CLightEntity*)(pFr->pLightOwner))->m_light.m_sName;
				PrintMessage("Requesting %s shadow update for %s, frame id = %d", 
					pFr->bOmniDirectionalShadow ? "Cube" : "2D", szName, GetFrameID());
			}
		}
}

ShadowMapFrustum * CLightEntity::GetShadowFrustum(int nId)
{
	if(m_pShadowMapInfo && nId<MAX_GSM_LODS_NUM) 
		return m_pShadowMapInfo->pGSM[nId];

	return NULL;
};

void CLightEntity::CheckUpdateCoverageMask()
{
	FUNCTION_PROFILER_3DENGINE;

  if(GetCVars()->e_cbuffer != 2)
    return;

  if(!m_bCoverageBufferDirty && m_arrCB[0])
    return;

	PrintMessage("CLightEntity::UpdateCoverageMask for %s ...", m_light.m_sName);

	const static float sCubeVector[6][7] = 
	{
		{1,0,0,  0,0,1, -90}, //posx
		{-1,0,0, 0,0,1,  90}, //negx
		{0,1,0,  0,0,-1, 0},  //posy
		{0,-1,0, 0,0,1,  0},  //negy
		{0,0,1,  0,1,0,  0},  //posz
		{0,0,-1, 0,1,0,  0},  //negz
	};

	GetCVars()->e_cbuffer_occluders_view_dist_ratio *= 100;

	for(int nS=0; nS<6; nS++)
	{
		if(!m_arrCB[nS])
			m_arrCB[nS] = new CCullBuffer();

		Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
		Vec3 vUp      = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
		Matrix33 matRot = Matrix33::CreateOrientation(vForward, vUp, DEG2RAD(sCubeVector[nS][6]));
		m_covCameras[nS] = GetCamera();
		m_covCameras[nS].SetMatrix(Matrix34(matRot, m_light.m_Origin));
		m_covCameras[nS].SetFrustum(GetCVars()->e_cbuffer_resolution, GetCVars()->e_cbuffer_resolution, gf_PI/2.f, 0.01f, m_light.m_fRadius);
		m_covCameras[nS].SetPosition(m_light.m_Origin);

		m_arrCB[nS]->BeginFrame(m_covCameras[nS]);

		Get3DEngine()->RenderPotentialOccluders(*m_arrCB[nS],m_covCameras[nS], true);
	}

	GetCVars()->e_cbuffer_occluders_view_dist_ratio /= 100;

	PrintMessagePlus(" updating objects ...");

	// force objects around to update affected lights list
	PodArray<SRNInfo> lstEntitiesInArea;
	if(Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->MoveObjectsIntoList(&lstEntitiesInArea, &m_WSBBox);
	Get3DEngine()->GetVisAreaManager()->MoveObjectsIntoList(&lstEntitiesInArea, m_WSBBox);
	for(int i=0; i<lstEntitiesInArea.Count(); i++)
    if(lstEntitiesInArea[i].pNode)
    {
//      lstEntitiesInArea[i].pNode->m_pAffectingLights = NULL;

      if(lstEntitiesInArea[i].pNode->m_pRNTmpData)
        Get3DEngine()->FreeRNTmpData(&lstEntitiesInArea[i].pNode->m_pRNTmpData);
    }

	PrintMessagePlus(" done");

	m_bCoverageBufferDirty = false;
}

void CLightEntity::DebugCoverageMask()
{
	FUNCTION_PROFILER_3DENGINE;

	int nSide = GetCVars()->e_cbuffer_lights_debug_side;
	if(nSide>=0 && nSide<6 && m_arrCB[nSide])
	{
		m_arrCB[nSide]->DrawDebug(2);
		Vec3 vPos = m_arrCB[nSide]->GetCamera().GetPosition();
		Vec3 vDir = m_arrCB[nSide]->GetCamera().GetViewdir();
		DrawLine(vPos, vPos+vDir*3);
	}
}


bool CLightEntity::IsBoxAffected(const AABB & box)
{
	// check distance
	if(m_light.m_Origin.GetDistance(box.GetCenter()) > m_light.m_fRadius + box.GetRadius())
		return false;

	for(int nS=0; nS<6; nS++)
	{
		if(m_arrCB[nS] && m_covCameras[nS].IsAABBVisible_E(box))
			if(m_arrCB[nS]->IsObjectVisible(box, eoot_OBJECT_TO_LIGHT, 0))
				return true;
	}

	return false;
}

void CLightEntity::OnCasterDeleted(IRenderNode * pCaster)
{
	if(!m_pShadowMapInfo)
		return;

  for(int nGsmId=0; nGsmId<MAX_GSM_LODS_NUM; nGsmId++)
  {
	  ShadowMapFrustum * pFr = m_pShadowMapInfo->pGSM[nGsmId];
	  if(pFr && pFr->pCastersList)
		  pFr->pCastersList->Delete(pCaster);
  }
}

void CLightEntity::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "LightEntity");
	pSizer->AddObject(this, sizeof(*this));
	
	for(int i=0; i<6; i++)
		if(m_arrCB[i])
			m_arrCB[i]->GetMemoryUsage(pSizer);

	if(m_pShadowMapInfo)
	{
		pSizer->AddObject(m_pShadowMapInfo, sizeof(*m_pShadowMapInfo));
		for(int i=0; i<MAX_GSM_LODS_NUM; i++)
		{
			if(m_pShadowMapInfo->pGSM[i])
			{
				pSizer->AddObject(m_pShadowMapInfo->pGSM[i], sizeof(*m_pShadowMapInfo->pGSM[i]));
				if(m_pShadowMapInfo->pGSM[i]->pCastersList)
				{
					pSizer->AddObject(m_pShadowMapInfo->pGSM[i]->pCastersList, 
						sizeof(*m_pShadowMapInfo->pGSM[i]->pCastersList) + sizeofVector(*m_pShadowMapInfo->pGSM[i]->pCastersList));
				}
			}
		}
	}
}
