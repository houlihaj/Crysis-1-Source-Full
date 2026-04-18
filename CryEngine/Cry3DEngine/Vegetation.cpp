////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   Vegetation.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading trees, buildings, ragister/unregister entities for rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "StatObj.h"
#include "ObjMan.h" 
#include "CullBuffer.h"
#include "3dEngine.h"
#include "Vegetation.h"

volatile int g_lockVegetationPhysics = 0;

#ifndef PI
#define PI 3.14159f
#endif

CVegetation::CVegetation() 
{
	Init();

	GetInstCount(eERType_Vegetation)++;
}

void CVegetation::Init()
{
	m_nObjectTypeID=0;
	m_vPos.Set(0,0,0);
	m_ucScale=0;
	m_pPhysEnt=0;
	m_pFoliage=0;
	m_ucAngle = 0;
	m_ucSunDotTerrain = 255;
	m_pRNTmpData = NULL;

  assert(sizeof(CVegetation) == 64 || sizeof(m_pRNTmpData) != 4);
}

//////////////////////////////////////////////////////////////////////////
void CVegetation::CalcMatrix( Matrix34 &tm, int * pFObjFlags )
{
  FUNCTION_PROFILER_3DENGINE;

	tm.SetIdentity();

	if (float fAngle = GetZAngle())
	{
		tm.SetRotationZ( fAngle );
		if(pFObjFlags)
			*pFObjFlags |= FOB_TRANS_ROTATE;
	}

	StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];

	if(vegetGroup.bAlignToTerrain)
	{
		Matrix33 m33; 
		m33.SetIdentity();
		Vec3 vTerrainNormal = GetTerrain()->GetTerrainSurfaceNormal(m_vPos, GetBBox().GetRadius());
		Vec3 vDir = Vec3(-1,0,0).Cross(vTerrainNormal);
		m33 = m33.CreateOrientation(vDir, -vTerrainNormal, 0);
		tm = m33*tm;
		if(pFObjFlags)
			*pFObjFlags |= FOB_TRANS_ROTATE;
	}

  float fScale = GetScale();
	if (fScale != 1.0f)
	{
		Matrix33 m33; 
		m33.SetIdentity();
		m33.SetScale(Vec3(fScale,fScale,fScale));
		tm = m33*tm;
		if(pFObjFlags)
			*pFObjFlags |= FOB_TRANS_SCALE;
	}

	tm.SetTranslation( m_vPos );
	if(pFObjFlags)
		*pFObjFlags |= FOB_TRANS_TRANSLATE;
}

//////////////////////////////////////////////////////////////////////////
AABB CVegetation::CalcBBox()
{
  AABB WSBBox = GetStatObj()->GetAABB();

	if (m_ucAngle)
	{
		Matrix34 tm;
		CalcMatrix( tm );
		WSBBox.SetTransformedAABB( tm, WSBBox );
	}
	else
	{
    float fScale = GetScale();
		WSBBox.min = m_vPos + WSBBox.min*fScale;
		WSBBox.max = m_vPos + WSBBox.max*fScale;
	}

  SetBBox(WSBBox);

  return WSBBox;
}

void CVegetation::CalcTerrainAdaption()
{
	FUNCTION_PROFILER_3DENGINE;

  assert(m_pRNTmpData);

  AABB WSBBox = GetBBox();

//	m_HMAGradients	=	Vec4(0.f,0.f,0.f,0.f);
	const	f32 minX		=	WSBBox.min.x;
	const	f32 minY		=	WSBBox.min.y;
	const	f32 maxX		=	WSBBox.max.x;
	const	f32 maxY		=	WSBBox.max.y;
	f32	HeightMax	=	m_vPos.z;
	f32	HeightMin	=	m_vPos.z;
	float H0,H1,H2,H3;
	H0=H1=H2=H3=m_vPos.z;
	if(minX>=0.f && maxX<CTerrain::GetTerrainSize())
	{
		const	f32 CenterX	=	(minX+maxX)*0.5f;
		H0	=	GetTerrain()->GetZApr(maxX,m_vPos.y);
		H1	=	GetTerrain()->GetZApr(minX,m_vPos.y);
//		m_HMAGradients.x	=	(H0-H1)/(maxX-minX);//linear
//		m_HMAGradients.z	=	((H0+H1)*0.5f-m_vPos.z)/((maxX-CenterX)*(maxX-CenterX));	//quadric
	}
	if(minY>=0.f && maxY<CTerrain::GetTerrainSize())
	{
		const	f32 CenterY	=	(minY+maxY)*0.5f;
		H2	=	GetTerrain()->GetZApr(m_vPos.x,maxY);
		H3	=	GetTerrain()->GetZApr(m_vPos.x,minY);
//		m_HMAGradients.y	=	(H0-H1)/(maxY-minY);
//		m_HMAGradients.w	=	((H0+H1)*0.5f-m_vPos.z)/((maxY-CenterY)*(maxY-CenterY));
	}

  CRNTmpData::SRNUserData & userData = m_pRNTmpData->userData;

	HeightMax		=		max(max(H0,H1),max(H2,H3));
	HeightMin		=		min(min(H0,H1),min(H2,H3));
	userData.m_HMARange	=		max(max(HeightMax-m_vPos.z,m_vPos.z-HeightMin),FLT_EPSILON);

	userData.m_HMAIndex	|=0xfff;	//to round up...ceil... the integer part
	userData.m_HMAIndex++;

	const float RANGE	=	static_cast<float>((1<<2)-1);
	int H0Idx		=		static_cast<int>((H0-m_vPos.z)/userData.m_HMARange*RANGE+RANGE+0.5f);//0.5for rounding
	int H1Idx		=		static_cast<int>((H1-m_vPos.z)/userData.m_HMARange*RANGE+RANGE+0.5f);
	int H2Idx		=		static_cast<int>((H2-m_vPos.z)/userData.m_HMARange*RANGE+RANGE+0.5f);
	int H3Idx		=		static_cast<int>((H3-m_vPos.z)/userData.m_HMARange*RANGE+RANGE+0.5f);

	userData.m_HMAIndex	=		(userData.m_HMAIndex&0xfffff000)|H0Idx|(H1Idx<<3)|(H2Idx<<6)|(H3Idx<<9);

//	m_WSBBox.min.z+=HeightMin-m_vPos.z;
//	m_WSBBox.max.z+=HeightMax-m_vPos.z;
}

float CVegetation_fSpriteSwitchState = 0.f;

bool CVegetation::AddVegetationIntoSpritesList(float fDistance, SSectorTextureSet * pTerrainTexInfo)
{
  StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];

  assert(vegetGroup.m_fSpriteSwitchDistUnScaled);

  // calculate switch to sprite distance 
  float fSpriteSwitchDist = vegetGroup.m_fSpriteSwitchDistUnScaled * GetScale();

  CVegetation_fSpriteSwitchState = 0.f;

  if(GetCVars()->e_dissolve)
  {
    if(vegetGroup.bUseSprites && fDistance >= fSpriteSwitchDist*0.75f)
    { // just register to be rendered as sprite and return
      CVegetation_fSpriteSwitchState = CLAMP(((fDistance/fSpriteSwitchDist)-0.75f)*8.f, 0.f, 2.f);

      float fScaleFade = (1.f-fDistance/(m_fWSMaxViewDist))*3.f;
      if (fScaleFade<=0)
        return true;
      if (fScaleFade>1.f)
        fScaleFade=1.f;

      SVegetationSpriteInfo si;
      si.ucSpriteAlphaTestRef = 25 + (uint8)(SATURATE(1.f-min(CVegetation_fSpriteSwitchState,fScaleFade))*225.f);
      si.pTerrainTexInfoForSprite = (vegetGroup.bUseTerrainColor && GetCVars()->e_vegetation_use_terrain_color) ? pTerrainTexInfo : NULL;
      si.pVegetation = this;
  
      if(fDistance > fSpriteSwitchDist*1.3f)
        si.dwAngle = ((COctreeNode*)m_pOcNode)->m_dwSpritesAngle;
      else
      {
        const Vec3 vCamPos = GetCamera().GetPosition();
        Vec3 vD = m_vPos - vCamPos;       
        Vec3 vCenter = vegetGroup.GetStatObj()->GetCenter() * GetScale();
        Vec3 vD1 = vCenter + vD;
        si.dwAngle = ((uint32)(cry_atan2f(vD1.x, vD1.y)*(255.0f/(2*PI)))) & 0xff;
      }

      GetObjManager()->m_lstFarObjects[m_nRenderStackLevel].Add(si);
      if (GetCVars()->e_bboxes)
        DrawBBox(GetBBox(), Col_Yellow);
      if(fDistance >= fSpriteSwitchDist)
        return true;
    }
  }
  else
  {
    if(vegetGroup.bUseSprites && fDistance >= fSpriteSwitchDist)
    { // just register to be rendered as sprite and return
      SVegetationSpriteInfo si;
      si.ucSpriteAlphaTestRef = 25;
      si.pTerrainTexInfoForSprite = (vegetGroup.bUseTerrainColor && GetCVars()->e_vegetation_use_terrain_color) ? pTerrainTexInfo : NULL;
      si.pVegetation = this;
      
      if(fDistance > fSpriteSwitchDist*1.3f)
        si.dwAngle = ((COctreeNode*)m_pOcNode)->m_dwSpritesAngle;
      else
      {
        const Vec3 vCamPos = GetCamera().GetPosition();
        Vec3 vD = m_vPos - vCamPos;       
        Vec3 vCenter = vegetGroup.GetStatObj()->GetCenter() * GetScale();
        Vec3 vD1 = vCenter + vD;
        si.dwAngle = ((uint32)(cry_atan2f(vD1.x, vD1.y)*(255.0f/(2*PI)))) & 0xff;
      }

      GetObjManager()->m_lstFarObjects[m_nRenderStackLevel].Add(si);
      if (GetCVars()->e_bboxes)
        DrawBBox(GetBBox(), Col_Yellow);
      return true;
    }
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetation::Render(const SRendParams& _EntDrawParams)
{
	FUNCTION_PROFILER_3DENGINE;

  // TODO: move to ocnode render
//  if(CVegetation::AddVegetationIntoSpritesList(_EntDrawParams.fDistance, _EntDrawParams.dwFObjFlags, _EntDrawParams.pTerrainTexInfo))
  //  return true;

  float fDistance = _EntDrawParams.fDistance;

  StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];

  float fScale = GetScale();

  // calculate switch to sprite distance 
  float fSpriteSwitchDist = vegetGroup.m_fSpriteSwitchDistUnScaled * fScale;

//	PrintMessage("CVegetation_fSpriteSwitchState = %.2f", CVegetation_fSpriteSwitchState);

  // render object in 3d
  if(_EntDrawParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP)
    fDistance = min(fDistance,fSpriteSwitchDist*0.99f);

  CStatObj * pBody = vegetGroup.GetStatObj();

  if(!pBody)
    return false;

  Get3DEngine()->CheckCreateRNTmpData(&m_pRNTmpData, this);
  assert(m_pRNTmpData);

  CRNTmpData::SRNUserData & userData = m_pRNTmpData->userData;

  // calculate bending amount
  if(!m_nRenderStackLevel && vegetGroup.fBending)
  {
    const float fStiffness( 0.5f ); 

    if(GetCVars()->e_wind && GetCVars()->e_vegetation_wind) 
    {
      const float fMaxBending( 5.0f );
      const float fWindResponse( fStiffness * fMaxBending / 10.0f );

      AABB WSBBox = GetBBox();
      Vec3 vCurrWind = Get3DEngine()->GetWind(WSBBox, _EntDrawParams.m_pVisArea != 0);
      vCurrWind *= 0.5f; // scale down to half

  		// bending - opt: disable at distance, based on amount of wind speed
	    float fWindSpeed = clamp_tpl( ((vCurrWind.x*vCurrWind.x + vCurrWind.y * vCurrWind.y))*0.02f, 0.0f, 0.6f); 
	    float fWindAtten = min( sqrtf( fSpriteSwitchDist *fSpriteSwitchDist *fSpriteSwitchDist ), 1.0f); //clamp_tpl( (1.0f - (fDistance/ (0.8f*fSpriteSwitchDist))) , 0.0f, 1.0f );

      // apply current wind
      float fFrameTime = GetTimer()->GetFrameTime();
      userData.m_vCurrentBending += fWindAtten * Vec2(vCurrWind) * (fFrameTime*fWindResponse);
      // apply restoration
      userData.m_vCurrentBending *= max(1.f - fFrameTime*fStiffness, 0.f);  

      // convert virtual bend to soft-limited real bend.		
      Vec2 vFinal = userData.m_vCurrentBending * fMaxBending / (userData.m_vCurrentBending.GetLength()+fMaxBending);
      userData.m_vFinalBending = vFinal;//*fScale; //(vFinal-userData.m_vFinalBending) * 10.0f * GetTimer()->GetFrameTime();
    }
    else
    { 
      // original bending
      const float fMaxBending = 2.f;

      userData.m_vCurrentBending *= max(1.f - GetTimer()->GetFrameTime()*fStiffness, 0.f);
      Vec3 vWind = Get3DEngine()->m_vWindSpeed * 0.5f;           
      userData.m_vFinalBending = userData.m_vCurrentBending + Vec2(vWind);
  
      userData.m_vFinalBending *= vegetGroup.fBending;

      // convert virtual bend fMaxBending soft-limited real bend.
      userData.m_vFinalBending *= fMaxBending / (userData.m_vFinalBending.GetLength()+fMaxBending);

      // convert relative to size.
      userData.m_vFinalBending /= pBody->GetRadiusVert()*fScale;
    }
    if(vegetGroup.bUseSprites)
      userData.m_vFinalBending *= (1.f - pow(fDistance/fSpriteSwitchDist, 4.f));
  }

  bool bUseTerrainColor(vegetGroup.bUseTerrainColor && GetCVars()->e_vegetation_use_terrain_color);

	SRendParams rParms;
	rParms.pMatrix = &userData.objMat;
  rParms.dwFObjFlags = userData.objMatFlags;
  rParms.nAfterWater = _EntDrawParams.nAfterWater;
  rParms.AmbientColor = _EntDrawParams.AmbientColor;
	rParms.fDistance = _EntDrawParams.fDistance;
	rParms.m_HMAData	=	userData.m_HMAIndex;
  rParms.nDLightMask = _EntDrawParams.nDLightMask;
  rParms.pRenderNode = _EntDrawParams.pRenderNode;
  rParms.nTechniqueID = _EntDrawParams.nTechniqueID;
  rParms.pInstInfo = _EntDrawParams.pInstInfo;

	if(bUseTerrainColor)
  {
    if(_EntDrawParams.pInstInfo)
    {
      AABB & aabb(_EntDrawParams.pInstInfo->aabb);
      Vec3 vTerrainNormal = GetTerrain()->GetTerrainSurfaceNormal(aabb.GetCenter(), aabb.GetRadius()/2);
      uchar ucSunDotTerrain	 = (uint8)(CLAMP((vTerrainNormal.Dot(Get3DEngine()->GetSunDirNormalized()))*255.f, 0, 255));
      rParms.AmbientColor.a *= (1.0f/255.f*ucSunDotTerrain);
    }
    else
		  rParms.AmbientColor.a *= (1.0f/255.f*m_ucSunDotTerrain);
  }
	else
		rParms.AmbientColor.a = vegetGroup.fBrightness;

	if(rParms.AmbientColor.a > 1.f)
		rParms.AmbientColor.a = 1.f;

  // set alpha blend
  if( vegetGroup.bUseAlphaBlending && !(_EntDrawParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && GetCVars()->e_vegetation_alpha_blend )
  {
    rParms.nRenderList = EFSLIST_GENERAL;
    if(vegetGroup.bUseSprites) // set fixed alpha
      rParms.fAlpha = 0.99f;
    else // fade alpha on distance
		{
			float fAlpha = min(1.f,(1.f - fDistance / m_fWSMaxViewDist)*6.f);
			rParms.fAlpha = min(0.99f,fAlpha);
		}
  }

  rParms.nMotionBlurAmount = 0;

  // Assign override material.
  rParms.pMaterial = vegetGroup.pMaterial;


  rParms.pShadowMapCasters = _EntDrawParams.pShadowMapCasters;
	rParms.dwFObjFlags |= (_EntDrawParams.dwFObjFlags & ~FOB_TRANS_MASK);

	//if it does not turn into a sprite, set the render quality to constant best
	rParms.fRenderQuality = vegetGroup.bUseSprites?min(1.f, max(1.f - fDistance/fSpriteSwitchDist,0.f)) : 1.f;
  rParms.nMaterialLayers = vegetGroup.nMaterialLayers;

  rParms.vBending = userData.m_vFinalBending;

	rParms.pTerrainTexInfo = _EntDrawParams.pTerrainTexInfo;

	if(vegetGroup.bUseTerrainColor && GetCVars()->e_vegetation_use_terrain_color && _EntDrawParams.pTerrainTexInfo)
		rParms.dwFObjFlags |= FOB_BLEND_WITH_TERRAIN_COLOR;

  rParms.dwFObjFlags |= FOB_VEGETATION;

	if (rParms.pFoliage = m_pFoliage)
		m_pFoliage->SetFlags(m_pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(rParms.nMaterialLayers&1) & IFoliage::FLAG_FROZEN);

	if(!_EntDrawParams.pInstInfo && GetCVars()->e_dissolve && !(_EntDrawParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && !vegetGroup.bUseAlphaBlending)
	{
		rParms.ppRNTmpData = &m_pRNTmpData;

		float f3dAmmount = CLAMP(1.f-(CVegetation_fSpriteSwitchState-1.f), 0.f, 1.f);
		rParms.nAlphaRef = max(_EntDrawParams.nAlphaRef, (byte)SATURATEB(255.f-f3dAmmount*255.f));
		if(rParms.nAlphaRef>0)
			rParms.dwFObjFlags |= FOB_DISSOLVE;
		if(f3dAmmount>0)
			pBody->Render(rParms);
	}
	else
  {
    rParms.dwFObjFlags &= ~FOB_DISSOLVE;
		pBody->Render(rParms);
  }

  return true;
}

IStatObj * CVegetation::GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId, Matrix34 * pMatrix, bool bReturnOnlyVisible ) 
{ 
	if(nPartId!=0)
		return 0;

	if(pMatrix)
	{
		Matrix34 tm;
		CalcMatrix( tm );
		*pMatrix = tm;
	}

  return GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj(); 
}

float CVegetation::GetMaxViewDist()
{
	StatInstGroup &group = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];
	CStatObj *pStatObj = (CStatObj*)(IStatObj*)group.pStatObj;
  if  (pStatObj)
	{
		if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
			return max(GetCVars()->e_view_dist_min, pStatObj->m_fObjectRadius*GetScale()*GetCVars()->e_view_dist_ratio_detail*group.fMaxViewDistRatio);

		return max(GetCVars()->e_view_dist_min, pStatObj->m_fObjectRadius*GetScale()*GetCVars()->e_view_dist_ratio_vegetation*group.fMaxViewDistRatio);
	}
	
	return 0;
}

void CVegetation::Physicalize( bool bInstant )
{
  FUNCTION_PROFILER_3DENGINE;

	CStatObj * pBody = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj();
	if(!pBody || !pBody->IsPhysicsExist() || (!gEnv->pSystem->IsDedicated() && pBody->GetRenderTrisCount()<8))
		return;

	bool bHideability = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].bHideability;
	bool bHideabilitySecondary = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].bHideabilitySecondary;

	//////////////////////////////////////////////////////////////////////////
	// Not create instance if no physical geometry.
	if(!pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
		//no bHidability check for the E3 demo - make all the bushes with MAT_OBSTRUCT things soft cover 
		//if(!(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT] && (bHideability || pBody->m_nSpines)))
		if(!(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT]))
			if(!(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE]))
				return;
	//////////////////////////////////////////////////////////////////////////

  AABB WSBBox = GetBBox();

	WriteLock lock(g_lockVegetationPhysics);
	int bNoOnDemand = 
		!GetCVars()->e_on_demand_physics || max(WSBBox.max.x-WSBBox.min.x, WSBBox.max.y-WSBBox.min.y)>Get3DEngine()->GetCVars()->e_on_demand_maxsize;
	bNoOnDemand = bNoOnDemand || (GetRndFlags()&ERF_PROCEDURAL)!=0;
	if (bNoOnDemand)
		bInstant = true;

  /*if (!bInstant)
  {
    pe_status_placeholder spc;
    if (m_pPhysEnt && m_pPhysEnt->GetStatus(&spc) && spc.pFullEntity)
      GetSystem()->GetIPhysicalWorld()->DestroyPhysicalEntity(spc.pFullEntity);

    pe_params_bbox pbb;
    pbb.BBox[0] = m_WSBBox.min;
    pbb.BBox[1] = m_WSBBox.max;
    pe_params_foreign_data pfd;
    pfd.pForeignData = (IRenderNode*)this;
    pfd.iForeignData = PHYS_FOREIGN_ID_STATIC;
		pfd.iForeignFlags = PFF_VEGETATION;

    if (!m_pPhysEnt)
      m_pPhysEnt = GetSystem()->GetIPhysicalWorld()->CreatePhysicalPlaceholder(PE_STATIC,&pbb);
    else
      m_pPhysEnt->SetParams(&pbb);
    m_pPhysEnt->SetParams(&pfd);
    return;
  }*/

  pBody->CheckLoaded();

	if (!bInstant)
	{
		gEnv->pPhysicalWorld->RegisterBBoxInPODGrid(&WSBBox.min);
		return;
	}

  

  // create new
	//pe_status_placeholder spc;
	//int bOnDemandCallback = 0;
	pe_params_pos pp;
	Matrix34 mtx;	CalcMatrix(mtx);
	pp.pMtx3x4 = &mtx;
	float fScale = GetScale();
	//pp.pos = m_vPos;
  //pp.q.SetRotationXYZ( Ang3(0,0,GetZAngle()) );
	//pp.scale = fScale;
  if (!m_pPhysEnt)
  {
    m_pPhysEnt = GetSystem()->GetIPhysicalWorld()->CreatePhysicalEntity(PE_STATIC, (1-bNoOnDemand)*5.0f, &pp,
			(IRenderNode*)this, PHYS_FOREIGN_ID_STATIC);        
    if(!m_pPhysEnt)
      return;
  }	else
		m_pPhysEnt->SetParams(&pp,1);
	//else if (bOnDemandCallback = m_pPhysEnt->GetStatus(&spc) && !spc.pFullEntity)
  // GetSystem()->GetIPhysicalWorld()->CreatePhysicalEntity(PE_STATIC,5.0f,0,0,0,-1,m_pPhysEnt);

	pe_action_remove_all_parts remove_all;
	m_pPhysEnt->Action(&remove_all,1);

  pe_geomparams params;
	params.density = 800;
	params.flags |= geom_break_approximation;
	params.scale = fScale;

	// collidable
  if(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
	{
		pe_params_ground_plane pgp;
		Vec3 bbox[2] = { pBody->GetBoxMin(),pBody->GetBoxMax() };
		pgp.iPlane = 0;
		pgp.ground.n.Set(0,0,1);
		(pgp.ground.origin = (bbox[0]+bbox[1])*0.5f).z -= (bbox[1].z-bbox[0].z)*0.49f;
		pgp.ground.origin *= fScale;
		m_pPhysEnt->SetParams(&pgp,1);

		if (!(m_dwRndFlags & ERF_PROCEDURAL)) 
		{
			params.idmatBreakable = pBody->m_idmatBreakable;
			if (pBody->m_bBreakableByGame)
				params.flags |= geom_manually_breakable;
		} else
			params.idmatBreakable = -1;
    m_pPhysEnt->AddGeometry(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT], &params, -1, 1);
	}

	phys_geometry *pgeom;
	params.density = 5;
	params.idmatBreakable = -1;
	if(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE] && pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT])
	{
		params.minContactDist = GetCVars()->e_foliage_stiffness;
		params.flags = geom_squashy|geom_colltype_obstruct; 
		m_pPhysEnt->AddGeometry(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT], &params, -1, 1);

		if (GetCVars()->e_phys_foliage>=2) 
		{
			params.density = 0;
			params.flags = geom_log_interactions;
			params.flagsCollider = 0;
			if (pBody->m_nSpines)
				params.flags |= geom_colltype_foliage_proxy;
			m_pPhysEnt->AddGeometry(pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE], &params, -1, 1);
		}
	}	
	else if ((pgeom=pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE]) || (pgeom=pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT]))
	{
		params.minContactDist = GetCVars()->e_foliage_stiffness;
		params.flags = geom_log_interactions|geom_squashy;
		if (pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT])
			params.flags |= geom_colltype_obstruct;
		if (pBody->m_nSpines)
			params.flags |= geom_colltype_foliage_proxy;
		m_pPhysEnt->AddGeometry(pgeom, &params, -1, 1);
	}

  if(bHideability)
  {
    pe_params_foreign_data  foreignData;
    m_pPhysEnt->GetParams(&foreignData);
    foreignData.iForeignFlags |= PFF_HIDABLE;
    m_pPhysEnt->SetParams(&foreignData,1);
  }

	if(bHideabilitySecondary)
	{
		pe_params_foreign_data  foreignData;
		m_pPhysEnt->GetParams(&foreignData);
		foreignData.iForeignFlags |= PFF_HIDABLE_SECONDARY;
		m_pPhysEnt->SetParams(&foreignData,1);
	}

	//PhysicalizeFoliage();
}

bool CVegetation::PhysicalizeFoliage(bool bPhysicalize, int iSource)
{
	CStatObj *pBody = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj();
	if(!pBody || !pBody->m_pSpines)
		return false;

	if (bPhysicalize)
	{
		Matrix34 mtx; CalcMatrix(mtx);
		if (pBody->PhysicalizeFoliage(m_pPhysEnt, mtx, m_pFoliage, GetCVars()->e_foliage_branches_timeout, iSource) &&
			!pBody->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
			((CStatObjFoliage*)m_pFoliage)->m_pVegInst = this;
	} else if (m_pFoliage)
		m_pFoliage->Release();
	else
		return false;

	return m_pFoliage!=0;
}

void CVegetation::ShutDown()
{
	Dephysicalize( );
	Get3DEngine()->FreeRenderNodeState(this); // Also does unregister entity.

	if(m_pRNTmpData)
		Get3DEngine()->FreeRNTmpData(&m_pRNTmpData);
	assert(!m_pRNTmpData);
}

CVegetation::~CVegetation()
{
	ShutDown();

	GetInstCount(eERType_Vegetation)--;

  Get3DEngine()->m_lstKilledVegetations.Delete(this);
}

void CVegetation::Dematerialize( )
{
}

void CVegetation::Dephysicalize( bool bKeepIfReferenced )
{
	// delete old physics
	WriteLock lock(g_lockVegetationPhysics);
	if(m_pPhysEnt && GetSystem()->GetIPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt,4*bKeepIfReferenced))
	{
		m_pPhysEnt = 0;
		if (m_pFoliage)
			m_pFoliage->Release();
	}
}

void CVegetation::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "Vegetation");
	pSizer->AddObject(this, sizeof(*this));
}

void CVegetation::UpdateRndFlags()
{
	const uint dwFlagsToUpdate = 
    ERF_CASTSHADOWMAPS | ERF_CASTSHADOWINTORAMMAP | ERF_HIDABLE | ERF_PICKABLE | ERF_SPEC_BITS_MASK | ERF_USE_TERRAIN_COLOR;
	m_dwRndFlags &= ~dwFlagsToUpdate;
	m_dwRndFlags |= GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].m_dwRndFlags & dwFlagsToUpdate;
	m_dwRndFlags |= ERF_OUTDOORONLY;
}

void CVegetation::UpdateInstanceBrightness()
{
  float fRadius = GetBBox().GetRadius();

	Vec3 vTerrainNormal = GetTerrain()->GetTerrainSurfaceNormal(m_vPos, fRadius);
	m_ucSunDotTerrain	 = (uint8)(CLAMP((vTerrainNormal.Dot(Get3DEngine()->GetSunDirNormalized()))*255.f, 0, 255));

	// TODO: take sky access into account
/*
	StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];
	assert(vegetGroup.m_fSpriteSwitchDistUnScaled);
	if(vegetGroup.m_fSpriteSwitchDistUnScaled && vegetGroup.GetStatObj())
	{
		float fSpriteSwitchDist = vegetGroup.m_fSpriteSwitchDistUnScaled * GetScale();
		float fLodRatioNorm = cry_sqrtf((vegetGroup.GetStatObj()->m_nLoadedLodsNum+1)*(GetCVars()->e_lod_ratio*fRadius)/fSpriteSwitchDist);
		m_ucLodRatio = (uint8)SATURATEB(fLodRatioNorm*100.f);
	}*/
}

void CVegetation::PreloadInstanceResources(Vec3 vPrevPortalPos, float fPrevPortalDistance, float fTime)
{
	IStatObj * pStatObj = GetEntityStatObj(0);
	if(!pStatObj || !pStatObj->GetRenderMesh())
		return;

	float fDist = fPrevPortalDistance + m_vPos.GetDistance(vPrevPortalPos);

	float fMaxViewDist = GetMaxViewDist();
	if(fDist<fMaxViewDist && fDist<GetCamera().GetFarPlane())
		pStatObj->PreloadResources(fDist,fTime,0);
}

const char *CVegetation::GetName() const
{
	return (GetObjManager()->m_lstStaticTypes.size() && GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj() )? 
		GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj()->GetFilePath() : "StatObjNotSet";
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CVegetation::GetMaterial(Vec3 * pHitPos)
{
	StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];

	if(vegetGroup.pMaterial)
		return vegetGroup.pMaterial;

	if (CStatObj *pBody = vegetGroup.GetStatObj())
		return pBody->GetMaterial();

	return NULL;
}

float CVegetation::GetLodForDistance(float fDistance)
{
	StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];

	assert(vegetGroup.m_fSpriteSwitchDistUnScaled);

	float fSpriteSwitchDist = vegetGroup.m_fSpriteSwitchDistUnScaled * GetScale();

	CStatObj * pBody = vegetGroup.GetStatObj();

//	if(fDistance > fSpriteSwitchDist)
//		if(((CStatObj*)pBody->GetLodObject(pBody->m_nLoadedLodsNum-1))->GetRenderTrisCount()>8)
	//		return -1;

	float fLod = (fDistance/fSpriteSwitchDist)*pBody->m_nLoadedLodsNum;

	if(fLod > pBody->m_nLoadedLodsNum-1)
		fLod = pBody->m_nLoadedLodsNum-1;

	return fLod;
}

float CVegetation::GetZAngle() const 
{ 
	if(m_ucAngle)
	{
		StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[m_nObjectTypeID];	
		return DEG2RAD( 360.0f/255.0f*m_ucAngle ); 
	}

	return 0;
}

void CVegetation::OnRenderNodeBecomeVisible()
{
  assert(m_pRNTmpData);
  m_pRNTmpData->userData.objMatFlags = 0;
  CalcMatrix( m_pRNTmpData->userData.objMat, &(m_pRNTmpData->userData.objMatFlags) );
  m_pRNTmpData->userData.fRadius = GetBBox().GetRadius();
  CalcTerrainAdaption();
}

void CVegetation::AddBending(Vec3 const& v) 
{ 
  if(m_pRNTmpData)
    m_pRNTmpData->userData.m_vCurrentBending += Vec2(v); 
}

const AABB CVegetation::GetBBox() const 
{ 
  return m_boxExtends.Get(m_vPos); 
}

void CVegetation::SetBBox( const AABB& WSBBox ) 
{ 
  m_boxExtends.Set(WSBBox, m_vPos); 
}

void SBoxExtends::Set(const AABB& aabb, const Vec3& vPos)
{
  Vec3 v;
  const float fRatio = 255.f/64.f;

  v = aabb.max - vPos;
  m_data[0] = (byte)SATURATEB(v.x*fRatio+1.f);
  m_data[1] = (byte)SATURATEB(v.y*fRatio+1.f);
  m_data[2] = (byte)SATURATEB(v.z*fRatio+1.f);

  v = vPos - aabb.min;
  m_data[3] = (byte)SATURATEB(v.x*fRatio+1.f);
  m_data[4] = (byte)SATURATEB(v.y*fRatio+1.f);
  m_data[5] = (byte)SATURATEB(v.z*fRatio+1.f);
}

AABB SBoxExtends::Get(const Vec3& vPos) const 
{
  AABB aabb; 
  const float fRatio = 64.f/255.f;

  aabb.max.x = vPos.x + m_data[0]*fRatio;
  aabb.max.y = vPos.y + m_data[1]*fRatio;
  aabb.max.z = vPos.z + m_data[2]*fRatio;

  aabb.min.x = vPos.x - m_data[3]*fRatio;
  aabb.min.y = vPos.y - m_data[4]*fRatio;
  aabb.min.z = vPos.z - m_data[5]*fRatio;

  return aabb;
}
