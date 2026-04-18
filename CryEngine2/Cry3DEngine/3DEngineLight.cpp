////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   3denginelight.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Light sources manager
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "AABBSV.h"
#include "terrain.h"
#include "LightEntity.h"
#include "ObjectsTree.h"
#include "Brush.h"

#ifndef PI
#define PI 3.14159f
#endif

void C3DEngine::RegisterLightSourceInSectors(CDLight * pDynLight)
{
	if(pDynLight->m_Flags & DLF_SUN || !m_pTerrain || !pDynLight->m_pOwner)
		return; 

  if(GetRenderer()->EF_IsFakeDLight(pDynLight))
    return;

  // pass to outdoor only outdoor lights and some indoor projectors 
  IVisArea * pLightArea = pDynLight->m_pOwner->GetEntityVisArea();
  if( !pLightArea || ((pDynLight->m_Flags & DLF_PROJECT) && pLightArea->IsConnectedToOutdoor()))
  {
    if(m_bShowTerrainSurface)
      m_pTerrain->RegisterLightMaskInSectors(pDynLight);

    if(m_pObjectsTree)
      m_pObjectsTree->AddLightSource(pDynLight);
  }

	if(m_pVisAreaManager)
		m_pVisAreaManager->AddLightSource(pDynLight);
}

ILightSource * C3DEngine::CreateLightSource()
{
	// construct new object
	CLightEntity * pLightEntity = new CLightEntity( );

	m_lstStaticLights.Add(pLightEntity);

	return pLightEntity;
}

void C3DEngine::DeleteLightSource(ILightSource * pLightSource)
{
	if(m_lstStaticLights.Delete((CLightEntity*)pLightSource) || pLightSource == m_pSun)
	{
		delete pLightSource;
		if(pLightSource == m_pSun)
			m_pSun = NULL;
	}
	else 
		assert(!"Light object not found");
}

void CLightEntity::Release(bool)
{ 
	Get3DEngine()->DeleteLightSource(this);
}

void CLightEntity::SetLightProperties(const CDLight & light) 
{ 
	if( m_light.m_fRadius < light.m_fRadius )
		m_bCoverageBufferDirty = true;

//	if( !m_light.m_Origin.IsEquivalent(light.m_Origin, 0.01f) )
	m_bCoverageBufferDirty = true;

	m_light = light; 
  m_light.m_fLightFrustumAngle = CLAMP(m_light.m_fLightFrustumAngle, 0.f, 60.f);

	if(!(m_light.m_Flags & DLF_PROJECT))
		m_light.m_fLightFrustumAngle = 90.f/2.f;

	m_light.m_pOwner = this;
}

const PodArray<CDLight*> * C3DEngine::GetStaticLightSources()
{
	// tmp solution since .h files are checked out
	static PodArray<CDLight*> lstLights;
	lstLights.Reset();

	for(int i=0; i<m_lstStaticLights.Count(); i++)
	{
		CDLight & light = m_lstStaticLights[i]->GetLightProperties();
		lstLights.Add( &light );
	}

	return &lstLights;
}

void C3DEngine::FindPotentialLightSources()
{
	FUNCTION_PROFILER_3DENGINE;

	const Vec3 vCamPos = GetCamera().GetPosition();

	for(int i=0; i<m_lstStaticLights.Count(); i++)
	{
		CLightEntity * pLightEntity = (CLightEntity *)m_lstStaticLights[i];		
		CDLight * pLight = &pLightEntity->m_light;

    int nRenderNodeMinSpec = (pLightEntity->m_dwRndFlags&ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
    if(!CheckMinSpec(nRenderNodeMinSpec))
      continue;

		float fEntDistance = (vCamPos-pLight->m_Origin).GetLength()*m_fZoomFactor;
		if(fEntDistance > pLightEntity->m_fWSMaxViewDist)
			continue;

    IMaterial * pMat = pLightEntity->GetMaterial();
    if(pMat)
      pLight->m_Shader = pMat->GetShaderItem();

    if(GetCamera().IsSphereVisible_F( Sphere(pLight->m_Origin,pLight->m_fRadius) ))
  		AddDynamicLightSource(*pLight,pLight->m_pOwner,1,pLightEntity->m_fWSMaxViewDist > 0.f ? fEntDistance / pLightEntity->m_fWSMaxViewDist : 0.f);
	}
}

void C3DEngine::ResetCasterCombinationsCache()
{
	for(int nSunInUse=0; nSunInUse<2; nSunInUse++)
	{
		// clear all allocated lists
		for(ShadowFrustumListsCache::iterator it = m_FrustumsCache[nSunInUse].begin(); it != m_FrustumsCache[nSunInUse].end(); ++it)
			(it->second)->Clear();

		// clear user counters
		for(ShadowFrustumListsCacheUsers::iterator it = m_FrustumsCacheUsers[nSunInUse].begin(); it != m_FrustumsCacheUsers[nSunInUse].end(); ++it)
			it->second = 0;
	}
}

void C3DEngine::DeleteAllStaticLightSources()
{
	for(int i=0; i<m_lstStaticLights.Count(); i++)
		delete m_lstStaticLights[i];
	m_lstStaticLights.Reset();

	m_pSun = NULL;
}

Ang3 ConvertProjAngles(const Ang3& vIn)
{
	Ang3 vOut;
	vOut.x = vIn.x;
	vOut.y = vIn.y;
	vOut.z = vIn.z;
	return vOut;
}

void C3DEngine::AddDynamicLightSource(const class CDLight & LSource, ILightSource *pEnt, int nEntityLightId, float fFadeout)
{
	assert(pEnt && _finite(LSource.m_Origin.x) && _finite(LSource.m_Origin.y) && _finite(LSource.m_fRadius));
	assert(LSource.IsOk());

  if((LSource.m_Flags & DLF_DISABLED) || (LSource.m_Flags & DLF_LOCAL) || (!GetCVars()->e_dynamic_light))
    return;

	if ((LSource.m_Flags & DLF_ONLY_FOR_HIGHSPEC) && (m_LightConfigSpec < CONFIG_HIGH_SPEC))
		return; // check spec settings

  if(m_lstDynLights.Count()==128)
    Warning("C3DEngine::AddDynamicLightSource: more than 128 dynamic light sources created");
  else if(m_lstDynLights.Count()>128)
    return;

	static ICVar * pPolygonMode = GetConsole()->GetCVar("r_PolygonMode");
	if(pPolygonMode && pPolygonMode->GetIVal()==2)
		return;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Detect sun case
	////////////////////////////////////////////////////////////////////////////////////////////////

	if(LSource.m_Flags & DLF_SUN && !(GetCVars()->e_cbuffer==2))
	{ // sun
		if(LSource.m_Color.r + LSource.m_Color.g + LSource.m_Color.b == 0 || !GetCVars()->e_sun)
			return; // sun disabled
	}
	else	if(GetCVars()->e_dynamic_light_frame_id_vis_test>1 && (GetCVars()->e_cbuffer != 2))
	{
    if(GetCVars()->e_dynamic_light_consistent_sort_order)
		  if(GetFrameID() - pEnt->GetDrawFrame(0) > MAX_FRAME_ID_STEP_PER_FRAME)
			  return;
	}

	if(GetCVars()->e_dynamic_light_frame_id_vis_test)
	{
		int nMaxReqursion = (LSource.m_Flags & DLF_THIS_AREA_ONLY) ? 2 : 3;
		if(!m_pObjManager || !m_pVisAreaManager || !m_pVisAreaManager->IsEntityVisAreaVisible(pEnt,nMaxReqursion, &LSource) )
		{
			if(LSource.m_Flags & DLF_SUN && m_pVisAreaManager && m_pVisAreaManager->m_bSunIsNeeded)
			{ // sun may be used in indoor even if outdoor is not visible
			}
			else
				return;			
		}
	}

  // distance fading
  const float fFadingRange = 0.25f;
	fFadeout -= (1.f - fFadingRange);
	fFadeout = fFadeout < 0.f ? 0.f : fFadeout;
	fFadeout = 1.f - fFadeout/fFadingRange;
  assert(fFadeout>0); // should be culled earlier

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Try to update present lsource
	////////////////////////////////////////////////////////////////////////////////////////////////

	for (int i=0; i<m_lstDynLights.Count(); i++)
	{
		if( m_lstDynLights[i].m_pOwner == pEnt )
		{
			// copy lsource (keep old CRenderObject)
			CRenderObject *pObj[4][4];
			memcpy(&pObj[0][0], &m_lstDynLights[i].m_pObject[0][0], sizeof(pObj));
			m_lstDynLights[i] = LSource;
			memcpy(&m_lstDynLights[i].m_pObject[0][0], &pObj[0][0], sizeof(pObj));

      // !HACK: Needs to decrement referene counter of shader because m_lstDynLights never release light sources
      if (LSource.m_Shader.m_pShader)
        LSource.m_Shader.m_pShader->Release();

			m_lstDynLights[i].m_pOwner = pEnt;

			// re-init start time
			m_lstDynLights[i].m_fStartTime = GetTimer()->GetCurrTime();
			m_lstDynLights[i].m_fLifeTime = LSource.m_fLifeTime;

			m_lstDynLights[i].m_ProjAngles = ConvertProjAngles(LSource.m_ProjAngles);

			// set base params
			m_lstDynLights[i].m_BaseOrigin = m_lstDynLights[i].m_Origin;

			if ((m_lstDynLights[i].m_Flags & DLF_SPECULAR_ONLY_FOR_HIGHSPEC) && (m_LightConfigSpec < CONFIG_HIGH_SPEC))
			{
				m_lstDynLights[i].m_SpecMult = 0;
			}

			m_lstDynLights[i].SetLightColor(ColorF(LSource.m_Color.r*fFadeout,LSource.m_Color.g*fFadeout,LSource.m_Color.b*fFadeout,LSource.m_Color.a));

			return;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Add new lsource into list and set some parameters
	////////////////////////////////////////////////////////////////////////////////////////////////

	m_lstDynLights.Add(LSource);

	// add ref to avoid shader deleting
	if (m_lstDynLights.Last().m_Shader.m_pShader)
		m_lstDynLights.Last().m_Shader.m_pShader->AddRef();
	m_lstDynLights.Last().m_Flags |= DLF_COPY;

	m_lstDynLights.Last().m_fStartTime = GetTimer()->GetCurrTime();
	m_lstDynLights.Last().m_fLifeTime = LSource.m_fLifeTime;

	if (!LSource.m_fEndRadius)
		m_lstDynLights.Last().m_fStartRadius = LSource.m_fRadius;

	m_lstDynLights.Last().m_pOwner = pEnt;

	m_lstDynLights.Last().m_ProjAngles = ConvertProjAngles(LSource.m_ProjAngles);

	// set base params
	m_lstDynLights.Last().m_BaseOrigin = m_lstDynLights.Last().m_Origin; 

	if ((m_lstDynLights.Last().m_Flags & DLF_SPECULAR_ONLY_FOR_HIGHSPEC) && (m_LightConfigSpec < CONFIG_HIGH_SPEC))
	{
		m_lstDynLights.Last().m_SpecMult = 0;
	}

	m_lstDynLights.Last().SetLightColor(ColorF(LSource.m_Color.r*fFadeout,LSource.m_Color.g*fFadeout,LSource.m_Color.b*fFadeout,LSource.m_Color.a));
}

void C3DEngine::PrepareLightSourcesForRendering_0()
{
  FUNCTION_PROFILER_3DENGINE;

  // reset lists of lsource pointers in sectors
  if(m_pTerrain)
  {
    bool bSunFound = m_lstDynLights.Count() && (m_lstDynLights.GetAt(0).m_Flags & DLF_SUN);
    m_pTerrain->SetSunLightMask(bSunFound ? 1 : 0);
    //ResetDLightMaskInSectors(bSunFound ? 1 : 0);
  }

  //	GetRenderer()->EF_ClearLightsList();
  m_lstDynLightsNoLight.Clear();

  bool bWarningPrinted = false;

  // update lmasks in terrain sectors
  if(m_nRenderStackLevel)

  { // do not delete lsources during recursion, becasue hmap lpasses are shared between levels
    for (int i=0; i<m_nRealLightsNum/*m_lstDynLights.Count()*/; i++)    
    {
      m_lstDynLights[i].m_Id = -1;
  //    GetRenderer()->EF_ADDDlight(&m_lstDynLights[i]);
    //  assert(m_lstDynLights[i].m_Id == i);
      //if(m_lstDynLights[i].m_Id != -1)
        //RegisterLightSourceInSectors(&m_lstDynLights[i]);
    }
  }
  else
  {
    IVisArea * pCameraVisArea = GetVisAreaFromPos(GetCamera().GetPosition());

    for (int i=0; i<m_lstDynLights.Count(); i++)    
    {
      m_lstDynLights[i].m_Id = -1;

      if( m_lstDynLights[i].m_pOwner && m_lstDynLights[i].m_pOwner->GetEntityVisArea())
      { // vis area lsource

        if(!GetCamera().IsSphereVisible_F( Sphere(m_lstDynLights[i].m_Origin,m_lstDynLights[i].m_fRadius) ))
        {
          FreeLightSourceComponents(&m_lstDynLights[i]);
          m_lstDynLights.Delete(i); i--;
          continue; // invisible
        }

        // check if light is visible thru light area portal cameras
        if(CVisArea * pArea = (CVisArea *)m_lstDynLights[i].m_pOwner->GetEntityVisArea())
          if(pArea->m_nRndFrameId == GetFrameID() && pArea != (CVisArea *)pCameraVisArea)
        {
          int nCam=0;
          for(; nCam<pArea->m_lstCurCameras.Count(); nCam++)
            if(pArea->m_lstCurCameras[nCam].IsSphereVisible_F( Sphere(m_lstDynLights[i].m_Origin,m_lstDynLights[i].m_fRadius) ))
              break;

          if(nCam==pArea->m_lstCurCameras.Count())
          { 
            FreeLightSourceComponents(&m_lstDynLights[i]);
            m_lstDynLights.Delete(i); i--;
            continue; // invisible
          }
        }

        // check if lsource is in visible area
        ILightSource * pEnt = m_lstDynLights[i].m_pOwner;
        if(!pEnt->IsLightAreasVisible() && pCameraVisArea != pEnt->GetEntityVisArea())
        {
          if(m_lstDynLights[i].m_Flags & DLF_THIS_AREA_ONLY)
          {
            if(pEnt->GetEntityVisArea())
            {
              int nRndFrameId = pEnt->GetEntityVisArea()->GetVisFrameId();
              if(GetFrameID() - pEnt->GetDrawFrame(0) > MAX_FRAME_ID_STEP_PER_FRAME)
              {
                FreeLightSourceComponents(&m_lstDynLights[i]);
                m_lstDynLights.Delete(i); i--;
                continue; // area invisible
              }
            }
            else
            {
              FreeLightSourceComponents(&m_lstDynLights[i]);
              m_lstDynLights.Delete(i); i--;
              continue; // area invisible
            }
          }
        }
      }
      else
      { // outdoor lsource
        if( !(m_lstDynLights[i].m_Flags & DLF_DIRECTIONAL) && !m_lstDynLights[i].m_pOwner->IsLightAreasVisible())
        {
          FreeLightSourceComponents(&m_lstDynLights[i]);
          m_lstDynLights.Delete(i); i--;
          continue; // outdoor invisible
        }
      }

      if(GetRenderer()->EF_IsFakeDLight(&m_lstDynLights[i]))
      { // ignored by renderer

        if(m_lstDynLights[i].m_Flags&DLF_FILL_LIGHT)
        {
          SFillLight fl;
          fl.m_vOrigin = m_lstDynLights[i].m_Origin;
          fl.m_fRadius = m_lstDynLights[i].m_fRadius;
          fl.m_fIntensity = m_lstDynLights[i].m_Color.r;
          fl.bNegative = (m_lstDynLights[i].m_Flags & DLF_NEGATIVE)!=0;
          GetRenderer()->EF_ADDFillLight(fl);
        }

        m_lstDynLightsNoLight.Add(m_lstDynLights[i]);
        m_lstDynLights.Delete(i); i--;
        continue; 
      }

      if(i >= MAX_LIGHTS_NUM)
      { // ignored by renderer

        assert(i >= MAX_LIGHTS_NUM);

        if(i >= MAX_LIGHTS_NUM && !bWarningPrinted && (GetMainFrameID()&7)==7)
        { // no more sources can be accepted by renderer
          Warning( "C3DEngine::PrepareLightSourcesForRendering: No more than %d real lsources allowed on the screen", (int)MAX_LIGHTS_NUM);
          bWarningPrinted = true;
        }

        FreeLightSourceComponents(&m_lstDynLights[i]);
        m_lstDynLights.Delete(i); i--;
        continue; 
      }

      /*if (m_lstDynLights[i].m_Shader.m_pShader!=0 && (m_lstDynLights[i].m_Shader.m_pShader->GetLFlags() & LMF_DISABLE))
      { // fake
        assert(0); // should not be called, but no problem
        FreeLightSourceComponents(&m_lstDynLights[i]);
        m_lstDynLights.Delete(i); i--;
        continue; 
      }*/

      //if(i != m_lstDynLights[i].m_Id)
      //  Warning( "C3DEngine::PrepareLightSourcesForRendering: Invalid light source id");

      if(m_lstDynLights[i].m_Flags & DLF_PROJECT 
        && m_lstDynLights[i].m_fLightFrustumAngle<90 // actual fov is twice bigger
        && m_lstDynLights[i].m_pLightImage
        && (m_lstDynLights[i].m_pLightImage->GetFlags() & FT_FORCE_CUBEMAP))
      { // prepare projector camera for frustum test
        m_arrLightProjFrustums.PreAllocate(i+1,i+1);
        CCamera & cam = m_arrLightProjFrustums[i];
        cam.SetPosition(m_lstDynLights[i].m_Origin);
        //				Vec3 Angles(m_lstDynLights[i].m_ProjAngles[1], 0, m_lstDynLights[i].m_ProjAngles[2]+90.0f);
        //			cam.SetAngle(Angles);
        Matrix34 entMat = ((ILightSource*)(m_lstDynLights[i].m_pOwner))->GetMatrix();
        Matrix33 matRot = Matrix33::CreateRotationVDir( entMat.GetColumn(0) );
        cam.SetMatrix(Matrix34(matRot,m_lstDynLights[i].m_Origin));
        cam.SetFrustum(1, 1, (m_lstDynLights[i].m_fLightFrustumAngle*2)/180.0f*PI, 0.1f,m_lstDynLights[i].m_fRadius);
      }

    }

    m_nRealLightsNum = m_lstDynLights.Count();
    m_lstDynLights.AddList(m_lstDynLightsNoLight);
/*
    for(int i=m_nRealLightsNum; i<m_lstDynLights.Count(); i++)
    { // ignored by renderer
      m_lstDynLights[i].m_Id = -1;
      GetRenderer()->EF_ADDDlight(&m_lstDynLights[i]);
      assert(m_lstDynLights[i].m_Id == -1);
    }
*/
    m_lstDynLightsNoLight.Clear();
  }

  if(GetCVars()->e_dynamic_light==2)
    for (int i=0; i<m_lstDynLights.Count(); i++)
    {
      float fSize = 0.05f*(sin(GetCurTimeSec()*10.f)+2.0f);
      DrawSphere(m_lstDynLights[i].m_Origin, fSize);
    }
}

int __cdecl C3DEngine__Cmp_LightPos(const void* v1, const void* v2)
{
  CDLight * p1 = ((CDLight*)v1);
  CDLight * p2 = ((CDLight*)v2);

  //from now the sun not need to have a shadowmap, but some part of the engine check only the first light to decide is there sun or not
  if((p1->m_Origin.x) > (p2->m_Origin.x))
    return -1;
  else if((p1->m_Origin.x) < (p2->m_Origin.x))
    return 1;

  return 0;
}

void C3DEngine::PrepareLightSourcesForRendering_1()
{
	FUNCTION_PROFILER_3DENGINE;

  //qsort(m_lstDynLights.GetElements(), m_nRealLightsNum, sizeof(CDLight), C3DEngine__Cmp_CastShadowFlag);

	if(!m_nRenderStackLevel)
  {
   // SortForShadowMask();
  }

	// reset lists of lsource pointers in sectors
  if(m_nRenderStackLevel)
  { // do not delete lsources during recursion, becasue hmap lpasses are shared between levels
    for (int i=0; i<m_nRealLightsNum/*m_lstDynLights.Count()*/; i++)    
    {
      m_lstDynLights[i].m_Id = -1;
      GetRenderer()->EF_ADDDlight(&m_lstDynLights[i]);
      assert(m_lstDynLights[i].m_Id == i);
      if(m_lstDynLights[i].m_Id != -1)
        RegisterLightSourceInSectors(&m_lstDynLights[i]);
    }
  }
  else
	{
		for (int i=0; i<m_nRealLightsNum && i<m_lstDynLights.Count(); i++)    
		{
			m_lstDynLights[i].m_Id = -1;
      m_lstDynLights[i].m_n3DEngineLightId = i;

      if(m_lstDynLights[i].m_Flags&DLF_SUN || GetCVars()->e_dynamic_light_consistent_sort_order)
        CheckAddLight(&m_lstDynLights[i]);

      if(m_lstDynLights[i].m_fRadius >= 0.5f)
      {
        assert(m_lstDynLights[i].m_fRadius >= 0.5f && !(m_lstDynLights[i].m_Flags & DLF_FAKE));
        RegisterLightSourceInSectors(&m_lstDynLights[i]);
      }
    }


		for(int i=m_nRealLightsNum; i<m_lstDynLights.Count(); i++)
		{ // ignored by renderer
			m_lstDynLights[i].m_Id = -1;
			GetRenderer()->EF_ADDDlight(&m_lstDynLights[i]);
			assert(m_lstDynLights[i].m_Id == -1);
		}
	}
}

void C3DEngine::InitShadowFrustums()
{
	FUNCTION_PROFILER_3DENGINE;

	for (int i=0; i<m_nRealLightsNum; i++)    
	{
		CDLight * pLight = &m_lstDynLights[i];
		CLightEntity * pLightEntity = (CLightEntity *)pLight->m_pOwner;		

		if(GetCVars()->e_shadows && pLight->m_Flags & DLF_CASTSHADOW_MAPS)
		{
			pLightEntity->UpdateGSMLightSourceShadowFrustum();

			if(pLightEntity->m_pShadowMapInfo)
			{
				pLight->m_pShadowMapFrustums = pLightEntity->m_pShadowMapInfo->pGSM;
				for(int nLod=0; nLod<MAX_GSM_LODS_NUM && pLight->m_pShadowMapFrustums[nLod]; nLod++)
					pLight->m_pShadowMapFrustums[nLod]->nDLightId = pLight->m_Id;
			}
		}

		IMaterial * pMat = pLightEntity->GetMaterial();
		if(pMat)
			pLight->m_Shader = pMat->GetShaderItem();

		if((GetCVars()->e_cbuffer==2) && !(pLight->m_Flags&DLF_SUN))
		{
			if(pLight->m_Flags&DLF_HAS_CBUFFER)
				pLightEntity->CheckUpdateCoverageMask();


			if( GetCVars()->e_cbuffer_lights_debug_side>=0 )//&& pLightEntity->GetRndFlags()&ERF_SELECTED )
				pLightEntity->DebugCoverageMask();
		}
	}

	//shadows frustums intersection test
	if (GetCVars()->e_shadows_debug == 4)
	{
		for (int i=0; i<m_nRealLightsNum; i++)
		{
			for (int j=(m_nRealLightsNum-1); j>=(i+1); j--)
			{

				CDLight * pLight = &m_lstDynLights[i];
				CLightEntity * pLightEntity1 = (CLightEntity *)pLight->m_pOwner;		

				pLight = &m_lstDynLights[j];
				CLightEntity * pLightEntity2 = (CLightEntity *)pLight->m_pOwner;		

				pLightEntity1->CheckFrustumsIntersect(pLightEntity2);
			}
		}
	}

	if(GetCVars()->e_shadows)
		ResetCasterCombinationsCache();
}

void C3DEngine::FreeLightSourceComponents(CDLight *pLight)
{
	FUNCTION_PROFILER_3DENGINE;

	for (int i=0; i<4; i++)
	{
		for (int j=0; j<4; j++)
		{
			if (pLight->m_pObject[i][j])
				pLight->m_pObject[i][j]->RemovePermanent();
			pLight->m_pObject[i][j]=0;
		}
	}

		//  delete pLight->m_pProjCamera;
		//pLight->m_pProjCamera=0;

		//if(pLight->m_pShader)
		//		SAFE_RELEASE(pLight->m_pShader);

}

int __cdecl C3DEngine__Cmp_CastShadowFlag(const void* v1, const void* v2)
{
	CDLight * p1 = ((CDLight*)v1);
	CDLight * p2 = ((CDLight*)v2);

	//from now the sun not need to have a shadowmap, but some part of the engine check only the first light to decide is there sun or not
	if((p1->m_Flags&DLF_SUN) > (p2->m_Flags&DLF_SUN))
		return -1;
	else if((p1->m_Flags&DLF_SUN) < (p2->m_Flags&DLF_SUN))
		return 1;

	if((p1->m_Flags&DLF_CASTSHADOW_MAPS) > (p2->m_Flags&DLF_CASTSHADOW_MAPS))
		return -1;
	else if((p1->m_Flags&DLF_CASTSHADOW_MAPS) < (p2->m_Flags&DLF_CASTSHADOW_MAPS))
		return 1;
/*
	ShadowMapFrustum * pFr1 = p1->m_pOwner->GetShadowFrustum();
	ShadowMapFrustum * pFr2 = p2->m_pOwner->GetShadowFrustum();
	int nCasters1 = pFr1 ? pFr1->pCastersList->Count() : 0;
	int nCasters2 = pFr2 ? pFr2->pCastersList->Count() : 0;

	if(nCasters1 > nCasters2)
		return -1;
	else if(nCasters1 < nCasters2)
		return 1;
*/
	if(p1->m_pOwner > p2->m_pOwner)
		return -1;
	else if(p1->m_pOwner < p2->m_pOwner)
		return 1;

	return 0;
}

typedef std::multimap<int, class CDLight*, std::greater<int> > t_mmapLSIntersect;
typedef std::pair<int, class CDLight*> t_pairLSIntersect;

//typedef std::multimap<uint, class CDLight> t_mmapLightGroups;
//typedef std::pair<uint, class CDLight> t_pairLightGroups;

//typedef std::pair<t_mmapGeomMeshes::iterator, t_mmapGeomMeshes::iterator> t_mmapGeomMeshRange;

void C3DEngine::SortForShadowMask()
{

  t_mmapLSIntersect mmapLSIntersect;

  mmapLSIntersect.clear();
  //mmapLSIntersect.reserve( m_lstDynLights.Count() );

  //sort lights by number of intersections
  for(int i=0; i<m_lstDynLights.Count(); i++)
  {
    CDLight* pLight1 = m_lstDynLights.Get(i);

    //skip sun and fake lights
    if ( (pLight1->m_Flags&DLF_SUN) || (pLight1->m_Flags&DLF_FAKE) || !(pLight1->m_Flags&DLF_CASTSHADOW_MAPS))
      continue;

    int numIntersect = 0;

    for(int j=0; j<m_lstDynLights.Count(); j++)
    {
      if(i==j)
        continue;

      CDLight* pLight2 = m_lstDynLights.Get(j);

      //skip sun and fake lights
      if ( (pLight2->m_Flags&DLF_SUN) || (pLight2->m_Flags&DLF_FAKE) || !(pLight2->m_Flags&DLF_CASTSHADOW_MAPS))
        continue;

      f32 fDistLS = (pLight2->m_Origin - pLight1->m_Origin).GetLength(); 

      if (fDistLS < (pLight2->m_fRadius + pLight1->m_fRadius))
        numIntersect++;
    }

    mmapLSIntersect.insert(t_pairLSIntersect(numIntersect,pLight1));
  }

  //FIX: reactivate multimap for lightgroups
  //t_mmapLightGroups LightGroups;
  //LightGroups.clear();

  

  //pack all valid light sources to the non-overlapped lightgroups

  TArray<CDLight*> LightGroups[32];
  for (int i=0; i<32; i++)
  {
    LightGroups[i].SetUse(0);
  }

  int nCurrentLG = -1;

  //add sun to separate channel 
  for(int i=0; i<m_lstDynLights.Count(); i++)
  {
    CDLight* pLight = m_lstDynLights.Get(i);

    if (pLight->m_Flags&DLF_SUN)
    {
      nCurrentLG++;
      assert(nCurrentLG<32);
      LightGroups[nCurrentLG].AddElem(pLight);
    }
  }


  //create light groups (share one shadow mask channel per light group) 
	for(t_mmapLSIntersect::const_iterator LSItor = mmapLSIntersect.begin(); LSItor!=mmapLSIntersect.end(); LSItor++ )
  {
    CDLight* pCurrLight = LSItor->second;
    //all created group
    bool bWasAdded = false;
    for(int nLG=0; nLG<=nCurrentLG; nLG++)
    {
      bool bWasIntersected = false;
      for(int i=0; i<LightGroups[nLG].Num(); i++)
      {
        CDLight* pLight = LightGroups[nLG][i];

        //separate intersection test for sun
        if (pLight->m_Flags&DLF_SUN)
        {
          bWasIntersected = true;
          break;
        }

        f32 fDistLS = (pLight->m_Origin - pCurrLight->m_Origin).GetLength(); 

        if (fDistLS < (pLight->m_fRadius + pCurrLight->m_fRadius))
        {
          bWasIntersected = true;
          break;
        }
      }

      if (!bWasIntersected)
      {
        bWasAdded = true;
        LightGroups[nCurrentLG].AddElem(pCurrLight);
        break;
      }
    }

    //start new light group
    if(!bWasAdded)
    {
      nCurrentLG++;
      assert(nCurrentLG<32);

      LightGroups[nCurrentLG].AddElem(pCurrLight);
    }

  }

  //////////////////////////////////////////////////////////////////////////

  //adjust lightgroups for the limitation of continious four-light light groups
  int nLGroup=0;
  while(nLGroup<8)
  {
    int i,j;
    
    int iFirstChan = nLGroup*4;
    int iLastChan = iFirstChan+3;
    int iLightsNum = 0;
    for (i=iFirstChan; i<=iLastChan; i++)
    {
      iLightsNum += LightGroups[i].Num();
    }
    //number of lights which need to be separated
    int nSepNum = (iLightsNum>4)?(iLightsNum%4):0;


    if (nSepNum>0)
    {
      //move lights to other light groups 
      for (i=iLastChan; i>=iFirstChan; i--)
      {
        int nMaxChanLights =  LightGroups[i].Num();
        int nMoveLight = min(nSepNum, nMaxChanLights);

        nCurrentLG++;
        assert(nCurrentLG<32);
        for(j=0; j<nMoveLight; j++)
        {
          LightGroups[nCurrentLG].AddElem( LightGroups[i][0] );
          LightGroups[i].Remove(0);
        }

        nSepNum -= nMoveLight;
        if (nSepNum<=0)
          break;
      }

      //if there was rearrangement then
      //start check for all light groups from the beginning
      nLGroup = 0;
      continue;
    }

    nLGroup++;

  }

  //generate actual final lights list
  PodArray<CDLight> m_lstNewDynLights;

  m_lstNewDynLights.clear();

  for (int i=0; i<32; i++)
  {
    for (int j=0; j<LightGroups[i].Num(); j++)
    {
      CDLight* pLight = LightGroups[i][j];
      pLight->m_nShadowMaskId = i/4;
      pLight->m_nShadowMaskChan = i%4;
      m_lstNewDynLights.Add(*pLight);
    }
  }


  //add all other light sources
  for(int i=0; i<m_lstDynLights.Count(); i++)
  {
    CDLight* pLight1 = m_lstDynLights.Get(i);

    if ((pLight1->m_Flags&DLF_FAKE) || !(pLight1->m_Flags&DLF_CASTSHADOW_MAPS))
    m_lstNewDynLights.Add(*pLight1);
  }


  //copy all to actual light list
  m_lstDynLights.Clear();
  m_lstDynLights.AddList(m_lstNewDynLights);

//  if(CV_e_)
  {
    float fTextPosX = 20;
    float fTextPosY = 20;
    float fTextStepY = 17;


	  float color[] = {1,1,1,1};

    for (int i=0; i<32; i++)
    {
      GetRenderer()->Draw2dLabel( fTextPosX, fTextPosY+=fTextStepY, 2, color, false, "Group #%8d | Lights #%8d", i, LightGroups[i].Num() );
    }
  }


  //Generate light list
  /*for(t_mmapLSIntersect::const_iterator LSItor = mmapLSIntersect.begin(); LSItor!=mmapLSIntersect.end(); LSItor++ )
  {
  }*/

};
//t_mmapLightGroups::interator LGitor =  LightGroups.equal_range(nLG);
/*
for(t_mmapLSIntersect::const_iterator LSItor2 = mmapLSIntersect->begin(); LSItor2!=mmapLSIntersect->end(); LSItor2++ )
{
  t_mmapLightGroups::interator LightGroups.find();

}
*/

void C3DEngine::UpdateLightSources()
{
	FUNCTION_PROFILER_3DENGINE;

  m_nRenderLightsNum = 0;

	// sort by cast shadow flag
	if(!m_nRenderStackLevel)
		qsort(m_lstDynLights.GetElements(), m_lstDynLights.Count(), sizeof(CDLight), C3DEngine__Cmp_CastShadowFlag);

	// process lsources
	float fCurrTime = GetTimer()->GetCurrTime();
	for(int i=0; i<m_lstDynLights.Count(); i++)
	{
		CDLight *pLight = &m_lstDynLights[i];

		if((pLight->m_fStartTime + pLight->m_fLifeTime+0.001f < fCurrTime) || (pLight->m_fStartTime > (fCurrTime+0.1f)))
		{
			// time is come to delete this lsource
			FreeLightSourceComponents(pLight);
			m_lstDynLights.Delete(i); i--;
			continue;
		}

		bool bDeleteNow = GetRenderer()->EF_UpdateDLight(pLight);
		//pLight->m_Color.Clamp();
		//pLight->m_SpecColor.Clamp();

		if(pLight->m_fLifeTime && !pLight->m_Shader.m_pShader)
		{ // evaluate radius
			float fAtten = 1.f - (fCurrTime - pLight->m_fStartTime) / pLight->m_fLifeTime;
			if(fAtten<0)
			{
				fAtten = 0;
				bDeleteNow = true;
			}
			else if(fAtten>1.f)
				fAtten = 1.f;
			pLight->m_fRadius = pLight->m_fBaseRadius*fAtten;
		}

		if(bDeleteNow)
		{
			// time is come to delete this lsource
			FreeLightSourceComponents(pLight);
			m_lstDynLights.Delete(i); i--;
			continue;
		}

    if(!m_nRenderStackLevel)
		  SetupLightScissors(&m_lstDynLights[i]);
	}
}

void C3DEngine::SetupLightScissors(CDLight * pLight)
{
	pLight->m_sX = 0;
	pLight->m_sY = 0;
	pLight->m_sWidth  = GetRenderer()->GetWidth();
	pLight->m_sHeight = GetRenderer()->GetHeight();

  Vec3 vViewVec = pLight->m_Origin - GetCamera().GetPosition();
  float fDistToLS =  vViewVec.GetLength();

  Vec2 vMin, vMax;
  if (fDistToLS<=pLight->m_fRadius)
  {
    //optimization when we are inside light frustum
    return;
  }

  float fRadiusSquared = pLight->m_fRadius * pLight->m_fRadius;

  //distance from Light Source center to the bound plane
  float fDistToBoundPlane = fRadiusSquared/fDistToLS;

  float fQuadEdge = sqrt_tpl(fRadiusSquared - fDistToBoundPlane * fDistToBoundPlane);

  vViewVec.SetLength(fDistToLS - fDistToBoundPlane);

  Vec3 vCenter = GetCamera().GetPosition() + vViewVec;

  Vec3 vRight  = GetCamera().GetViewMatrix().GetRow(2).normalized();
  Vec3 vUp	  = -GetCamera().GetViewMatrix().GetRow(0).normalized();

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

  vMin = Vec2(1,1);
  vMax = Vec2(0,0);

  Matrix44 mProj, mView;
  GetRenderer()->GetProjectionMatrix(mProj.GetData());
  GetRenderer()->GetModelViewMatrix(mView.GetData());
  Matrix44 mComposite = mView * mProj;

  for (int i=0; i<4; i++)
  {
    if (GetCVars()->e_scissor_debug)
    {
      if (GetRenderer()->GetIRenderAuxGeom()!=NULL)
      {
        GetRenderer()->GetIRenderAuxGeom()->DrawPoint( pBRectVertices[i] ,RGBA8(0xff,0xff,0xff,0xff),10);

        int32 nPrevVert = (i-1)<0?3:(i-1);

        GetRenderer()->GetIRenderAuxGeom()->DrawLine( pBRectVertices[nPrevVert], RGBA8(0xff,0xff,0x0,0xff), pBRectVertices[i], RGBA8(0xff,0xff,0x0,0xff), 3.0f);
      }

    }
    Vec4 vScreenPoint = Vec4(pBRectVertices[i], 1.0) * mComposite;

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

    vMin.x = min(vMin.x, vWin.x);
    vMin.y = min(vMin.y, vWin.y);
    vMax.x = max(vMax.x, vWin.x);
    vMax.y = max(vMax.y, vWin.y);
  }

  float fWidth = GetRenderer()->GetWidth();
  float fHeight = GetRenderer()->GetHeight();

  pLight->m_sX = (short)(vMin.x * fWidth);
  pLight->m_sY = (short)((1-vMax.y) * fHeight);
  pLight->m_sWidth = (short)((vMax.x-vMin.x) * fWidth);
  pLight->m_sHeight = (short)((vMax.y-vMin.y) * fHeight);
}

int __cdecl C3DEngine__Cmp_DLightAmount(const void* v1, const void* v2)
{
	DLightAmount * p1 = ((DLightAmount*)v1);
	DLightAmount * p2 = ((DLightAmount*)v2);

	if(p1->fAmount > p2->fAmount)
		return -1;
	else if(p1->fAmount < p2->fAmount)
		return 1;

	return 0;
}

uint C3DEngine::GetFullLightMask()
{
	uint nRes=0;
	for(int n=0; n<min(m_nRealLightsNum,m_lstDynLights.Count()); n++)
	{
		const int nId = m_lstDynLights[n].m_Id;
		assert(nId>=0);
		nRes |= (1<<nId);
	}

	return nRes;
}

float C3DEngine::GetLightAmount(CDLight * pLight, const AABB & objBox)
{
	// find amount of light
  float fDist = cry_sqrtf(Distance::Point_AABBSq(pLight->m_Origin, objBox));
	float fLightAttenuation = (pLight->m_Flags & DLF_DIRECTIONAL) ? 1.f : 1.f - (fDist) / (pLight->m_fRadius);
	if(fLightAttenuation<0)
		fLightAttenuation=0;

	float fLightAmount = 
		(pLight->m_Color.r+pLight->m_Color.g+pLight->m_Color.b)*0.233f + 
		(pLight->m_SpecMult)*0.1f;

	return fLightAmount*fLightAttenuation;
}

uint C3DEngine::BuildLightMask( const AABB & objBox )
{
  CVisArea * pArea = (CVisArea *)GetVisAreaFromPos(objBox.GetCenter());

  COctreeNode * pObjectsTree = pArea ? pArea->m_pObjectsTree : m_pObjectsTree;

  if(!pObjectsTree)
    return 0;

  pObjectsTree = pObjectsTree->FindNodeContainingBox(objBox);

  if(!pObjectsTree)
    return 0;

  return BuildLightMask( objBox, pObjectsTree->GetAffectingLights(), pArea, false );
}

uint C3DEngine::BuildLightMask( const AABB & objBox, PodArray<CDLight*> * pAffectingLights, CVisArea * pObjArea, bool bObjOutdoorOnly)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(m_nRenderStackLevel>=0 && "C3DEngine::BuildLightMask call is allowed only during C3DEngine::Render call");

  static PodArray<DLightAmount> lstLightInfos; lstLightInfos.Clear();

  uint nDLightMask = 0;

	// make list of affecting light sources
  for(int i=0; i<pAffectingLights->Count(); i++)
  {
    CDLight * pLight = pAffectingLights->GetAt(i);
    ILightSource * pLightOwner = pLight->m_pOwner;
    CVisArea * pLightArea = (CVisArea*)pLightOwner->GetEntityVisArea();
    bool bThisAreaOnlyLight = (pLight->m_Flags & DLF_THIS_AREA_ONLY) != 0;

    assert(pLight->m_Id>=0 || !GetCVars()->e_dynamic_light_consistent_sort_order);
    assert(pLightOwner);

		if(pLightArea && bThisAreaOnlyLight && pObjArea != pLightArea)
			if(!pObjArea || !pObjArea->IsPortal())
				continue; // different areas

		if(pLight->m_Flags&DLF_INDOOR_ONLY && bObjOutdoorOnly)
      continue; // indoor-only light is not affecting outdoor-only objects

		// calculate amount of light for object
		if( float fLightAmount = GetLightAmount(pLight, objBox) )
		{
      // check projector frustum
      if(  pLight->m_Flags & DLF_PROJECT && pLight->m_fLightFrustumAngle<90 // actual fov is twice bigger
        && pLight->m_pLightImage!=0 && (pLight->m_pLightImage->GetFlags() & FT_FORCE_CUBEMAP))
      { 
        if (!m_arrLightProjFrustums[pLight->m_n3DEngineLightId].IsAABBVisible_E( objBox ))
          continue;
      }

      // check sphere/bbox intersection
      if(!Overlap::Sphere_AABB(Sphere(pLight->m_Origin, pLight->m_fRadius), objBox))
        continue;

      // check object to light visibility
      if(pObjArea || pLightArea)
        if(BuildLightMask_CheckPortals( pObjArea, pLightArea, pLight, objBox ))
          continue;

      Get3DEngine()->CheckAddLight(pLight);

			DLightAmount la;
			la.pLight = pLight;
			la.fAmount = fLightAmount;
			lstLightInfos.Add(la);
		}
	}

	if(!lstLightInfos.Count())
		return 0; // no light sources found

	// sort by light amount
	qsort(&lstLightInfos[0], lstLightInfos.Count(), sizeof(lstLightInfos[0]), C3DEngine__Cmp_DLightAmount);

  // cull lights by coverage buffer
  /*if(GetCVars()->e_cbuffer==2 && pObj->GetRenderNodeType() != eERType_Light)
  {
    const char * szName = pObj->GetName();

    if(!pObj->m_pAffectingLights)
      pObj->m_pAffectingLights = Get3DEngine()->GetAffectingLights(pObj->m_WSBBox, true);

    // limit number of effective light sources
    int nRealCount = 0;
    for(int n=0; nRealCount<GetCVars()->e_max_entity_lights && n<lstLightInfos.Count(); n++)
    {
      CDLight * pLight = lstLightInfos[n].pLight;

      if(!(pLight->m_Flags&DLF_SUN))
        if(pObj->m_pAffectingLights->Find(pLightOwner)<0)
          continue;

      nDLightMask |= (1<<lstLightInfos[n].pLight->m_Id);
      nRealCount++;
    }
  }
  else*/
  {
    // limit number of effective light sources (except in e_debug_lights mode)
		int nLimitNum = GetCVars()->e_debug_lights ? 16 : GetCVars()->e_max_entity_lights;
    int nMaxNum = min( nLimitNum, lstLightInfos.Count());
    for(int n=0; n<nMaxNum; n++)
      nDLightMask |= (1<<lstLightInfos[n].pLight->m_Id);
  }

  return nDLightMask;
}

bool C3DEngine::BuildLightMask_CheckPortals( CVisArea * pObjArea, CVisArea * pLightArea, CDLight * pLight, const AABB & objBox )
{
  bool bThisAreaOnlyLight = (pLight->m_Flags & DLF_THIS_AREA_ONLY) != 0;

  if(pObjArea) // entity is indoor
  { 
    if( pObjArea != pLightArea )
    {	// try also neighbor areas
      if(pLightArea)
      { // check areas connectivity
        int nSearchDepth;
        if(pLight->m_Flags & DLF_PROJECT && !bThisAreaOnlyLight)
          nSearchDepth = 5 + int(pLightArea->IsPortal()); // allow projector to go depther
        else
          nSearchDepth = 2 + int(bThisAreaOnlyLight==false) + int(pLightArea->IsPortal());

        bool bNearFound = pObjArea->FindVisArea(pLightArea, nSearchDepth, true);
        if(!bNearFound)
          return true; // areas do not much
      }
      else // outdoor light
      { // check connection to outdoor
        if(!pObjArea->IsConnectedToOutdoor() || !pObjArea->m_bAffectedByOutLights)
          return true;
      }

      // construct frustum from light pos and portal, check that object is visible from light position
      if(!(bThisAreaOnlyLight && pLightArea) && pObjArea && !pObjArea->IsPortal() && 
        GetCVars()->e_shadows && pLight->m_Flags & DLF_CASTSHADOW_MAPS)
      {
        Shadowvolume sv;

        int p = 0;
        for(; p<pObjArea->m_lstConnections.Count(); p++)
        {
          CVisArea * pPortal = pObjArea->m_lstConnections[p];

          assert( pPortal->IsPortal() );

          if(pPortal == pLightArea)
            break; // light is inside of near portal - frustum construction is not possible

          if(int nConnNum = pPortal->m_lstConnections.Count())
          {
            if( (nConnNum==1 && !pLightArea) || 
              pPortal->m_lstConnections[0] == pLightArea || (nConnNum>1 && pPortal->m_lstConnections[1] == pLightArea))
            {
              NAABB_SV::AABB_ShadowVolume(pLight->m_Origin, pPortal->m_boxArea, sv, pLight->m_fRadius);
              if(NAABB_SV::Is_AABB_In_ShadowVolume(sv, objBox))
                break;
            }
          }
        }
        if(p == pObjArea->m_lstConnections.Count())
          return true; // object is not visible from light position
      }
    }
  }
  else if(pLightArea) // entity is outside, light is inside
  {
    if(!bThisAreaOnlyLight && pLightArea->IsConnectedToOutdoor() && pLight->m_Flags & DLF_PROJECT)
    {
      if(!pLightArea->IsPortal())
      {
        Shadowvolume sv;

        int p = 0;
        for(; p<pLightArea->m_lstConnections.Count(); p++)
        {
          CVisArea * pPortal = pLightArea->m_lstConnections[p];

          if(pPortal->m_lstConnections.Count() == 1)
          { // portal to outdoor found
            NAABB_SV::AABB_ShadowVolume(pLight->m_Origin, pPortal->m_boxArea, sv, pLight->m_fRadius);
            if(NAABB_SV::Is_AABB_In_ShadowVolume(sv, objBox))
              break;
          }
        }

        if(p == pLightArea->m_lstConnections.Count())
          return true; // object is not visible from light position
      }
    }
    else 
      return true; // by default indoor lsource should not affect outdoor entity
  }

  return false;
}

void C3DEngine::RemoveEntityLightSources(IRenderNode * pEntity)
{
	for (int i=0; i<m_lstDynLights.Count(); i++)    
	{
		if(m_lstDynLights[i].m_pOwner == pEntity)
		{
			FreeLightSourceComponents(&m_lstDynLights[i]);
			m_lstDynLights.Delete(i);
			i--;
		}
	}
	
	for (int i=0; i<m_lstStaticLights.Count(); i++)    
	{
		if(m_lstStaticLights[i] == pEntity)
		{
			CLightEntity * pLightEntity = (CLightEntity*)m_lstStaticLights[i];
			m_lstStaticLights.Delete(i);
			if(pEntity == m_pSun)
				m_pSun = NULL;
			i--;
		}
	}
}

float C3DEngine:: GetLightAmountInRange(const Vec3 &pPos, float fRange, bool bAccurate)
{ 
  static Vec3 pPrevPos(0, 0, 0);  
  static float fPrevLightAmount = 0.0f, fPrevRange = 0.0f;

  CVisAreaManager *pVisAreaMan( GetVisAreaManager() );
  if( !pVisAreaMan )
  {
    return 0.0f;
  }

  // Return cached result instead ( should also check if lights position/range changes - but how todo efficiently.. ? )
  if( fPrevLightAmount != 0.0f && fPrevRange == fRange && pPrevPos.GetSquaredDistance( pPos ) < 0.25f )
  {
    return fPrevLightAmount;
  }
  
  CVisArea *pCurrVisArea( static_cast<CVisArea *>( Get3DEngine()->GetVisAreaFromPos( pPos ) ) );
  bool bOutdoor( !pCurrVisArea );

  Vec3 pAmb;
  if( bOutdoor )
  {
    // In outdoors use sky color
    pAmb = Get3DEngine()->GetSkyColor();
  }
  else
  {
    pAmb = pCurrVisArea->m_vAmbColor;
  }
  
  // Take into account ambient lighting first
  float fAmbLength( pAmb.len2() );
  if( fAmbLength > 1.0f )
  {
    // Normalize color (for consistently working with HDR/LDR)
    pAmb /= cry_sqrtf( fAmbLength );
  }

  float fLightAmount( ( pAmb.x + pAmb.y + pAmb.z ) / 3.0f );     

  for(int l(0); l < m_lstStaticLights.Count(); ++l)    
  {     
    CDLight &pLight = m_lstStaticLights[l]->GetLightProperties();
    
    if( (pLight.m_Flags & DLF_FAKE) )
    {
      // Fake light source not needed for light amount
      continue;
    }

    CVisArea *pLightVisArea( static_cast<CVisArea *>( m_lstStaticLights[l]->GetEntityVisArea() ) );

    if( pCurrVisArea != pLightVisArea && (pLight.m_Flags & DLF_THIS_AREA_ONLY) )
    {
      // Not same vis area, skip
      continue;
    }           

    float fAtten = 1.0f;
    float fDist = 0.0f;
    
    // Check if light comes from sun (only directional light source..), if true then no attenuation is required
    if( !(pLight.m_Flags & DLF_DIRECTIONAL) )
    {
      fDist = pLight.m_Origin.GetSquaredDistance(pPos);      
      fAtten = (1.0f - (fDist - fRange * fRange) / (pLight.m_fRadius * pLight.m_fRadius)) ;

      // No need to proceed if attenuation coefficient is too small
      if( fAtten < 0.01f ) 
      {
       continue;
      }

      fAtten = clamp_tpl<float>(fAtten, 0.0f, 1.0f);         
    }

    Vec3 pLightColor( pLight.m_Color.r, pLight.m_Color.g, pLight.m_Color.b );
    float fLightLength( pLightColor.len2() );
    if( fLightLength > 1.0f )
    {
      // Normalize color (for consistently working with HDR/LDR)
      pLightColor /= cry_sqrtf(fLightLength);
    }

    float fCurrAmount( ((pLightColor.x + pLightColor.y + pLightColor.z)/3.0f) * fAtten );         
    if(fCurrAmount < 0.01f)
    {
      // no need to proceed if current lighting amount is too small
      continue;
    }  

    if(bAccurate && (pLight.m_Flags & DLF_CASTSHADOW_MAPS)) 
    {      
      // Ray-cast to each of the 6 'edges' of a sphere, get an average of visibility and use it
      // to attenuate current light amount
      
      float fHitAtten( 0 );
      ray_hit pHit;

      const int nSamples = 6;
      Vec3 pOffsets[nSamples]=
      { 
        Vec3( fRange, 0, 0 ),
        Vec3( -fRange, 0, 0 ),
        Vec3( 0, fRange, 0 ),
        Vec3( 0, -fRange, 0 ),
        Vec3( 0, 0, fRange ),
        Vec3( 0, 0, -fRange ),
      };

      const uint32 nEntQueryFlags( rwi_any_hit | rwi_stop_at_pierceable );
      const int32 nObjectTypes( ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid );

      for(int s(0); s < nSamples; ++s )
      {
        Vec3 pCurrPos( pPos - pOffsets[s] );
        Vec3 pDir(pLight.m_Origin - pCurrPos);           

        if(GetPhysicalWorld()->RayWorldIntersection(pPos, pDir, nObjectTypes, nEntQueryFlags, &pHit, 1))  
        {
          fHitAtten += 1.0f;
        }
      }

      // Average the n edges hits and attenuate light amount factor
      fHitAtten = 1.0f - (fHitAtten / (float) nSamples );  
      fCurrAmount *= fHitAtten; 
    }    

     // Sum up results  
    fLightAmount += fCurrAmount;
  }

  fLightAmount = clamp_tpl<float>(fLightAmount, 0.0f, 1.0f);

  // Cache results
  pPrevPos = pPos;
  fPrevLightAmount = fLightAmount;
  fPrevRange = fRange;

	return fLightAmount;
}
  
ILightSource * C3DEngine::GetSunEntity() 
{ 
	return m_pSun; 
}

PodArray<struct ILightSource*> * C3DEngine::GetAffectingLights(const AABB & bbox, bool bAllowSun)
{
	static PodArray<struct ILightSource*> lstAffectingLights; lstAffectingLights.Clear();

	// fill temporary list
	for(int i=0; i<m_lstStaticLights.Count(); i++)
	{
		CLightEntity * pLightEntity = (CLightEntity *)m_lstStaticLights[i];		
		CDLight * pLight = &pLightEntity->m_light;
		if(pLight->m_Flags&DLF_SUN)
			continue;

		bool bHasCBuffer = (pLight->m_Flags&DLF_HAS_CBUFFER) != 0;
	
		// check occlusion
		if(bHasCBuffer)
			pLightEntity->CheckUpdateCoverageMask();

		if(!bHasCBuffer || pLightEntity->IsBoxAffected(bbox))
		{
			ILightSource * p = (ILightSource*)pLightEntity;
			lstAffectingLights.Add(p);
		}
	}

	// search for same cached list
	for(int nListId=0; nListId<m_lstAffectingLightsCombinations.Count(); nListId++)
	{
		PodArray<struct ILightSource*> * pCachedList = m_lstAffectingLightsCombinations[nListId];
		if(pCachedList->Count() == lstAffectingLights.Count() &&
			!memcmp(pCachedList->GetElements(),lstAffectingLights.GetElements(), lstAffectingLights.Count()*sizeof(ILightSource*)))
			return pCachedList;
	}

	// if not found in cache - create new
	PodArray<struct ILightSource*> * pNewList = new PodArray<struct ILightSource*>;
	pNewList->AddList(lstAffectingLights);
	m_lstAffectingLightsCombinations.Add(pNewList);

	return pNewList;
}

void C3DEngine::UregisterLightFromAccessabilityCache(ILightSource * pLight)
{
	// search for same cached list
	for(int nListId=0; nListId<m_lstAffectingLightsCombinations.Count(); nListId++)
	{
		PodArray<struct ILightSource*> * pCachedList = m_lstAffectingLightsCombinations[nListId];
		pCachedList->Delete(pLight);
	}
}

void C3DEngine::CheckAddLight(CDLight*pLight)
{
  if(pLight->m_Id<0)
  {
    GetRenderer()->EF_ADDDlight(pLight);
    GetRenderLightsNum()++;
    assert(pLight->m_Id>=0);
  }
}