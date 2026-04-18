////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   decals.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw, create decals on the world
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <ICryAnimation.h>

#include "DecalManager.h"
#include "3dEngine.h"
#include <IStatObj.h>
#include "ObjMan.h"
#include "terrain.h"
#include "RenderMeshMerger.h"
#include "RenderMeshUtils.h"
#include "VoxMan.h"

#ifndef RENDER_MESH_TEST_DISTANCE
	#define RENDER_MESH_TEST_DISTANCE (0.2f)
#endif


CDecalManager::CDecalManager() 
{ 
  m_nCurDecal = 0; 
  memset(m_arrbActiveDecals,0,sizeof(m_arrbActiveDecals)); 
}

CDecalManager::~CDecalManager()
{
}

bool CDecalManager::AdjustDecalPosition( CryEngineDecalInfo & DecalInfo, bool bMakeFatTest )
{
	Matrix34 objMat, objMatInv;
	Matrix33 objRot, objRotInv;

	CStatObj * pEntObject = (CStatObj *)DecalInfo.ownerInfo.GetOwner(objMat);
	if(!pEntObject || !pEntObject->GetRenderMesh() || !pEntObject->GetRenderTrisCount())
		return false;

	objRot = Matrix33(objMat);
	objRot.NoScale(); // No scale.
	objRotInv = objRot;
	objRotInv.Invert();

	float fWorldScale = objMat.GetColumn(0).GetLength(); // GetScale
	float fWorldScaleInv = 1.0f / fWorldScale;
	
	// transform decal into object space 
	objMatInv = objMat;
	objMatInv.Invert();

	// put into normal object space hit direction of projection
	Vec3 vOS_HitDir = objRotInv.TransformVector(DecalInfo.vHitDirection).GetNormalized();

	// put into position object space hit position
	Vec3 vOS_HitPos = objMatInv.TransformPoint(DecalInfo.vPos);
	vOS_HitPos -= vOS_HitDir*RENDER_MESH_TEST_DISTANCE*fWorldScaleInv;

  IMaterial * pMat = DecalInfo.ownerInfo.pRenderNode ? DecalInfo.ownerInfo.pRenderNode->GetMaterial() : NULL;

  Vec3 vOS_OutPos(0,0,0), vOS_OutNormal(0,0,0), vTmp;
  IRenderMesh * pRM = pEntObject->GetRenderMesh();

  AABB aabbRNode;
  pRM->GetBBox(aabbRNode.min, aabbRNode.max);
  Vec3 vOut(0,0,0);
  if(!Intersect::Ray_AABB(Ray(vOS_HitPos,vOS_HitDir), aabbRNode, vOut))
    return false;
  
	if (!pRM || !pRM->GetSysVertBuffer() || !pRM->GetVertCount())
		return false;

	if (RayRenderMeshIntersection(pRM, vOS_HitPos, vOS_HitDir, vOS_OutPos, vOS_OutNormal, false,0, pMat) )
  {
		// now check that none of decal sides run across edges
    Vec3 srcp = vOS_OutPos + 0.01f*fWorldScaleInv*vOS_OutNormal; /// Rise hit point a little bit above hit plane.
    Vec3 vDecalNormal = vOS_OutNormal;
		float fMaxHitDistance = 0.02f*fWorldScaleInv;

		// get decal directions
		Vec3 vRi(0,0,0), vUp(0,0,0);
		if(fabs(vOS_OutNormal.Dot(Vec3(0,0,1)))>0.999f)
		{ // horiz surface
			vRi = Vec3(0,1,0);
			vUp = Vec3(1,0,0);
		}
		else
		{
			vRi = vOS_OutNormal.Cross(Vec3(0,0,1));
			vRi.Normalize();
			vUp = vOS_OutNormal.Cross(vRi);
			vUp.Normalize();
		}

		vRi*=DecalInfo.fSize*0.65f;
		vUp*=DecalInfo.fSize*0.65f;

    if(	!bMakeFatTest || (
      RayRenderMeshIntersection(pRM, srcp+vUp, -vDecalNormal, vTmp, vTmp,true,fMaxHitDistance, pMat) &&
      RayRenderMeshIntersection(pRM, srcp-vUp, -vDecalNormal, vTmp, vTmp,true,fMaxHitDistance, pMat) &&
      RayRenderMeshIntersection(pRM, srcp+vRi, -vDecalNormal, vTmp, vTmp,true,fMaxHitDistance, pMat) &&
      RayRenderMeshIntersection(pRM, srcp-vRi, -vDecalNormal, vTmp, vTmp,true,fMaxHitDistance, pMat) ))
    {
      DecalInfo.vPos = objMat.TransformPoint(vOS_OutPos + vOS_OutNormal*0.001f*fWorldScaleInv);
      DecalInfo.vNormal = objRot.TransformVector(vOS_OutNormal);
      return true;
    }
  }
	return false;
}

struct HitPosInfo
{
	HitPosInfo() { memset(this,0,sizeof(HitPosInfo)); }
	Vec3 vPos,vNormal;
	float fDistance;	
};

int __cdecl CDecalManager__CmpHitPos(const void* v1, const void* v2)
{
	HitPosInfo * p1 = (HitPosInfo*)v1;
	HitPosInfo * p2 = (HitPosInfo*)v2;

	if(p1->fDistance > p2->fDistance)
		return 1;
	else if(p1->fDistance < p2->fDistance)
		return -1;

	return 0;
}

bool CDecalManager::RayRenderMeshIntersection(IRenderMesh * pRenderMesh, const Vec3 & vInPos, const Vec3 & vInDir, 
																							Vec3 & vOutPos, Vec3 & vOutNormal,bool bFastTest,float fMaxHitDistance,IMaterial * pMat)
{
	SRayHitInfo hitInfo;
	hitInfo.bUseCache = GetCVars()->e_decals_hit_cache!=0;
	hitInfo.bInFirstHit = bFastTest;
	hitInfo.inRay.origin = vInPos;
	hitInfo.inRay.direction = vInDir.GetNormalized();
	hitInfo.inReferencePoint = vInPos;
	hitInfo.fMaxHitDistance = fMaxHitDistance;
	bool bRes = CRenderMeshUtils::RayIntersection( pRenderMesh,hitInfo,pMat );
	vOutPos = hitInfo.vHitPos;
	vOutNormal = hitInfo.vHitNormal;
	return bRes;
}

bool CDecalManager::SpawnHierarhical( const CryEngineDecalInfo & rootDecalInfo, float fMaxViewDistance, CTerrain* pTerrain, CDecal* pCallerManagedDecal )
{
  // decal on terrain or simple decal on always static object
  if(!rootDecalInfo.ownerInfo.pRenderNode)
    return Get3DEngine()->CreateDecalOnStatObj( rootDecalInfo, NULL ); 

  bool bSuccess = false;

  AABB decalBoxWS;
  float fSize = rootDecalInfo.fSize;
  decalBoxWS.max = rootDecalInfo.vPos+Vec3(fSize,fSize,fSize);
  decalBoxWS.min = rootDecalInfo.vPos-Vec3(fSize,fSize,fSize);

  for(int nEntitySlotId=0; nEntitySlotId<16; nEntitySlotId++)
  {
    CStatObj * pStatObj = NULL;
    Matrix34 entSlotMatrix; entSlotMatrix.SetIdentity();
    if(pStatObj = (CStatObj *)rootDecalInfo.ownerInfo.pRenderNode->GetEntityStatObj(nEntitySlotId, -1, &entSlotMatrix, true))
    {
      if (pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND)
      {
        if(int nSubCount = pStatObj->GetSubObjectCount())
        { // spawn decals on stat obj sub objects
          for(int nSubId=0; nSubId<nSubCount; nSubId++)
          {
            IStatObj::SSubObject &subObj = pStatObj->SubObject(nSubId);
            if (subObj.pStatObj && !subObj.bHidden && subObj.nType == STATIC_SUB_OBJECT_MESH)
            {
              Matrix34 subObjMatrix = entSlotMatrix * subObj.tm;
              AABB subObjAABB = AABB::CreateTransformedAABB(subObjMatrix, subObj.pStatObj->GetAABB());
              if(Overlap::AABB_AABB(subObjAABB, decalBoxWS))
              {
                CryEngineDecalInfo decalInfo = rootDecalInfo;
                decalInfo.ownerInfo.nRenderNodeSlotId = nEntitySlotId;
                decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nSubId;
                decalInfo.bAdjustPos = true;
                bSuccess |= Get3DEngine()->CreateDecalOnStatObj(decalInfo, NULL);
              }
            }
          }
        }
      }
      else
      {
        AABB subObjAABB = AABB::CreateTransformedAABB(entSlotMatrix, pStatObj->GetAABB());
        if(Overlap::AABB_AABB(subObjAABB, decalBoxWS))
        {
          CryEngineDecalInfo decalInfo = rootDecalInfo;
          decalInfo.ownerInfo.nRenderNodeSlotId = nEntitySlotId;
          decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1; // no childs
          decalInfo.bAdjustPos = true;
          bSuccess |= Get3DEngine()->CreateDecalOnStatObj(decalInfo, NULL);
        }
      }
    }
    else if(ICharacterInstance * pChar = rootDecalInfo.ownerInfo.pRenderNode->GetEntityCharacter(nEntitySlotId, &entSlotMatrix))
    { // spawn decals on CGA components
      ISkeletonPose* pSkeletonPose = pChar->GetISkeletonPose();
      uint32 numJoints = pSkeletonPose->GetJointCount();

      // spawn decal on every sub-object intersecting decal bbox
      for(int nJointId=0; nJointId<numJoints; nJointId++)
      {
        IStatObj * pStatObj = pSkeletonPose->GetStatObjOnJoint(nJointId);

        if(pStatObj && !(pStatObj->GetFlags()&STATIC_OBJECT_HIDDEN) && pStatObj->GetRenderMesh())
        {      
          assert(!pStatObj->GetSubObjectCount());

          Matrix34 tm34 = entSlotMatrix*Matrix34(pSkeletonPose->GetAbsJointByID(nJointId));
          AABB objBoxWS = AABB::CreateTransformedAABB(tm34, pStatObj->GetAABB());
          //				DrawBBox(objBoxWS);
          //			DrawBBox(decalBoxWS);
          if(Overlap::AABB_AABB(objBoxWS, decalBoxWS))
          {
            CryEngineDecalInfo decalInfo = rootDecalInfo;
            decalInfo.ownerInfo.nRenderNodeSlotId = nEntitySlotId;
            decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nJointId;
            decalInfo.bAdjustPos = true;
            bSuccess |= Get3DEngine()->CreateDecalOnStatObj(decalInfo, NULL);
          }
        }
      }
    }

    if(rootDecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_VoxelObject)
    {
      CryEngineDecalInfo decalInfo = rootDecalInfo;
      decalInfo.ownerInfo.nRenderNodeSlotId = 0;
      decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1; // no childs
      decalInfo.bAdjustPos = false;
      bSuccess |= Get3DEngine()->CreateDecalOnStatObj(decalInfo, NULL);
      break;
    }
  }

  return bSuccess;
}

bool CDecalManager::Spawn( CryEngineDecalInfo DecalInfo, float fMaxViewDistance, CTerrain* pTerrain, CDecal* pCallerManagedDecal )
{
	FUNCTION_PROFILER_3DENGINE;

  Vec3 vCamPos = GetSystem()->GetViewCamera().GetPosition();

	// do not spawn if too far
	float fZoom = GetObjManager() ? GetObjManager()->m_fZoomFactor : 1.f;
	float fDecalDistance = DecalInfo.vPos.GetDistance(vCamPos);
  if( !pCallerManagedDecal && (fDecalDistance > fMaxViewDistance || fDecalDistance*fZoom>DecalInfo.fSize*ENTITY_DECAL_DIST_FACTOR*3.f) )
    return false;

	// do not spawn new decals if they could overlap the existing and similar ones
  if(!pCallerManagedDecal && !GetCVars()->e_decals_overlapping && DecalInfo.fSize && !DecalInfo.bSkipOverlappingTest)
  { 
    for(int i=0; i<DECAL_COUNT; i++)
    {
      if(m_arrbActiveDecals[i])
      {
				// skip overlapping check if decals are very different in size
				float fSizeRatio = m_arrDecals[i].m_fWSSize / DecalInfo.fSize;
				if(fSizeRatio > 0.5f && fSizeRatio < 2.f && m_arrDecals[i].m_nGroupId != DecalInfo.nGroupId)
        {
          float fDist = m_arrDecals[i].m_vWSPos.GetSquaredDistance(DecalInfo.vPos);
					if(fDist < sqr(m_arrDecals[i].m_fWSSize / 1.5f + DecalInfo.fSize / 2.0f))
						return true;
        }
      }
    }
  }

	if(GetCVars()->e_decals > 1)
		DrawSphere(DecalInfo.vPos,DecalInfo.fSize);

	// project decal to visible geometry
	if(DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() != eERType_VoxelObject && DecalInfo.fSize<=GetCVars()->e_decals_wrap_around_min_size)
		if(!AdjustDecalPosition( DecalInfo, DecalInfo.fSize<=GetCVars()->e_decals_wrap_around_min_size ))
			return false;

	// update lifetime for near decals under control by the decal manager
	if( !pCallerManagedDecal )
	{
		if(DecalInfo.fSize > 1 && GetCVars()->e_decals_neighbor_max_life_time)
		{ // force near decals to fade faster
			float fCurrTime = GetTimer()->GetCurrTime();
			for(int i=0; i<DECAL_COUNT; i++)
				if(m_arrbActiveDecals[i] && m_arrDecals[i].m_nGroupId != DecalInfo.nGroupId)
				{
					if(m_arrDecals[i].m_vWSPos.GetSquaredDistance(DecalInfo.vPos) < sqr(m_arrDecals[i].m_fWSSize / 1.5f + DecalInfo.fSize / 2.0f))
						if((m_arrDecals[i]).m_fLifeBeginTime < fCurrTime-0.1f)
							if(m_arrDecals[i].m_fLifeTime > GetCVars()->e_decals_neighbor_max_life_time)
                if(m_arrDecals[i].m_fLifeTime < 10000) // decals spawn by cut scenes need to stay
  								m_arrDecals[i].m_fLifeTime = GetCVars()->e_decals_neighbor_max_life_time;
				}
		}

		// loop position in array
		m_nCurDecal=(m_nCurDecal+1)&(DECAL_COUNT-1);
		//if(m_nCurDecal>=DECAL_COUNT)
		//	m_nCurDecal=0;
	}

	// create reference to decal which is to be filled 
	CDecal& newDecal( pCallerManagedDecal ? *pCallerManagedDecal : m_arrDecals[ m_nCurDecal ] );

	// free old pRM
	newDecal.FreeRenderData();

  newDecal.m_nGroupId = DecalInfo.nGroupId;

	// get material if specified
	newDecal.m_pMaterial = 0;
	newDecal.m_isTempMat = false;
	if( DecalInfo.szTextureName && DecalInfo.szTextureName[0] != '\0' )
	{
		newDecal.m_pMaterial = GetMaterialForDecalTexture( DecalInfo.szTextureName );
		newDecal.m_isTempMat = newDecal.m_pMaterial ? true : false;
	}
	else if( DecalInfo.szMaterialName && DecalInfo.szMaterialName[0] != '0' )
	{
		newDecal.m_pMaterial = GetMatMan()->LoadMaterial( DecalInfo.szMaterialName, false,true );
		if( !newDecal.m_pMaterial )
		{
			newDecal.m_pMaterial = GetMatMan()->LoadMaterial( "Materials/Decals/Default",true,true );
			newDecal.m_pMaterial->AddRef();
			Warning( "CDecalManager::Spawn: Specified decal material \"%s\" not found!\n", DecalInfo.szMaterialName );
		}
	}

	newDecal.m_sortPrio = DecalInfo.sortPrio;

	// set up user defined decal basis if provided
	bool useDefinedUpRight( false );
	Vec3 userDefinedUp;
	Vec3 userDefinedRight;
	if( DecalInfo.pExplicitRightUpFront )
	{
		userDefinedRight = DecalInfo.pExplicitRightUpFront->GetColumn( 0 );
		userDefinedUp = DecalInfo.pExplicitRightUpFront->GetColumn( 1 );
		DecalInfo.vNormal = DecalInfo.pExplicitRightUpFront->GetColumn( 2 );
		useDefinedUpRight = true;
	}

	// just in case
	DecalInfo.vNormal.NormalizeSafe();

	// remember object we need to follow
	newDecal.m_ownerInfo.nRenderNodeSlotId = DecalInfo.ownerInfo.nRenderNodeSlotId;
	newDecal.m_ownerInfo.nRenderNodeSlotSubObjectId = DecalInfo.ownerInfo.nRenderNodeSlotSubObjectId;

	newDecal.m_vWSPos = DecalInfo.vPos;
	newDecal.m_fWSSize = DecalInfo.fSize;

	// If owner entity and object is specified - make decal use entity geometry
	float fObjScale = 1.f;

  Matrix34 objMat;
  Matrix33 worldRot;
  IStatObj * pStatObj = DecalInfo.ownerInfo.GetOwner(objMat);
  if (pStatObj)
  {
    worldRot = Matrix33(objMat);
    objMat.Invert();
  }

  bool bSpawnOnVegetationWithBending = false;
  if(DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation)
  {
//    CVegetation * pVeg = (CVegetation*)DecalInfo.ownerInfo.pRenderNode;
//    StatInstGroup & rGroup = GetObjManager()->m_lstStaticTypes[pVeg->m_nObjectTypeID];
  //  if(rGroup.fBending>0)
      bSpawnOnVegetationWithBending = true;

    // Check owner material ID (need for decals vertex modifications)
    SRayHitInfo hitInfo;
    memset(&hitInfo,0,sizeof(hitInfo));
    Vec3 vHitPos = objMat.TransformPoint(DecalInfo.vPos);
    // put hit normal into the object space
    Vec3 vRayDir = DecalInfo.vHitDirection.GetNormalized()*worldRot;
    hitInfo.inReferencePoint = vHitPos;
    hitInfo.inRay.origin = vHitPos - vRayDir*4.0f;
    hitInfo.inRay.direction = vRayDir;
    hitInfo.bInFirstHit = false;
    hitInfo.bUseCache = true;
    if (pStatObj->RayIntersection(hitInfo, 0))
      newDecal.m_ownerInfo.nMatID = hitInfo.nHitMatID;
  }

	if(DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.nRenderNodeSlotId>=0 && (DecalInfo.fSize>GetCVars()->e_decals_wrap_around_min_size || pCallerManagedDecal || bSpawnOnVegetationWithBending))
	{ 
		newDecal.m_eDecalType = eDecalType_OS_OwnersVerticesUsed;

    IRenderMesh * pSourceRenderMesh = NULL;
   
    if(pStatObj)
		  pSourceRenderMesh = pStatObj->GetRenderMesh();
    else if(DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_VoxelObject)
    {
      CVoxelObject * pVox = (CVoxelObject*)DecalInfo.ownerInfo.pRenderNode;
      pSourceRenderMesh = pVox->GetRenderMesh(0);
      objMat = pVox->GetMatrix();
      objMat.Invert();
    }

    if(!pSourceRenderMesh)
      return false;

		// transform decal into object space 
		Matrix33 objRotInv = Matrix33(objMat);
		objRotInv.NoScale();

		if( useDefinedUpRight )
		{
			userDefinedRight = objRotInv.TransformVector( userDefinedRight ).GetNormalized();
			userDefinedUp = objRotInv.TransformVector( userDefinedUp ).GetNormalized();
			assert( fabsf( DecalInfo.vNormal.Dot( -DecalInfo.vHitDirection.GetNormalized() ) - 1.0f ) < 1e-4f );
		}

    // make decals smaller but longer if hit direction is near perpendicular to surface normal
    float fSizeModificator = 0.25f + 0.75f*fabs(DecalInfo.vHitDirection.GetNormalized().Dot(DecalInfo.vNormal));

		// put into normal object space hit direction of projection
		DecalInfo.vNormal = -objRotInv.TransformVector((DecalInfo.vHitDirection - DecalInfo.vNormal*0.25f).GetNormalized());			

		if(!DecalInfo.vNormal.IsZero())
			DecalInfo.vNormal.Normalize();

		// put into position object space hit position
		DecalInfo.vPos = objMat.TransformPoint(DecalInfo.vPos);

		// find object scale
    float fObjScale = 1.f;
		Vec3 vTest(0,0,1.f);
		vTest = objMat.TransformVector(vTest);
		fObjScale = 1.f/vTest.len();

		if(fObjScale<0.01f)
			return false;

		// transform size into object space
		DecalInfo.fSize/=fObjScale;

    DecalInfo.fSize *= fSizeModificator;

		// make decal geometry
    newDecal.m_pRenderMesh = MakeBigDecalRenderMesh(pSourceRenderMesh, DecalInfo.vPos, DecalInfo.fSize, DecalInfo.vNormal, newDecal.m_pMaterial, pStatObj ? pStatObj->GetMaterial() : NULL);

		if(!newDecal.m_pRenderMesh)
			return false; // no geometry found
	}	
	else if(!DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.pDecalReceivers && (DecalInfo.fSize>GetCVars()->e_decals_wrap_around_min_size || pCallerManagedDecal))
	{
		newDecal.m_eDecalType = eDecalType_WS_Merged;

		assert(!newDecal.m_pRenderMesh);

		// put into normal hit direction of projection
		DecalInfo.vNormal = -DecalInfo.vHitDirection;			
		if(!DecalInfo.vNormal.IsZero())
			DecalInfo.vNormal.Normalize();

    Vec3 vSize(DecalInfo.fSize*1.333f,DecalInfo.fSize*1.333f,DecalInfo.fSize*1.333f);
    AABB decalAABB(DecalInfo.vPos-vSize,DecalInfo.vPos+vSize);

		// build list of affected brushes
		PodArray<SRenderMeshInfoInput> lstRMI;
		for(int nObj=0; nObj<DecalInfo.ownerInfo.pDecalReceivers->Count(); nObj++)
		{
			IRenderNode * pDecalOwner = DecalInfo.ownerInfo.pDecalReceivers->Get(nObj)->pNode;
			Matrix34 objMat;		
      if(IStatObj * pEntObject = pDecalOwner->GetEntityStatObj(DecalInfo.ownerInfo.nRenderNodeSlotId, 0, &objMat))
      {
			  SRenderMeshInfoInput rmi;
			  rmi.pMesh = pEntObject->GetRenderMesh();
			  rmi.pMat = pEntObject->GetMaterial();
			  rmi.mat = objMat;

        if(rmi.pMesh && rmi.pMesh->GetChunks())
        {
          AABB transAABB = AABB::CreateTransformedAABB(rmi.mat,pEntObject->GetAABB());
          if(Overlap::AABB_AABB(decalAABB,transAABB))
            lstRMI.Add(rmi);
        }
        else if(int nSubObjCount = pEntObject->GetSubObjectCount())
        { // multi sub objects
          for(int nSubObj=0; nSubObj<nSubObjCount; nSubObj++)
          {
            IStatObj::SSubObject * pSubObj = pEntObject->GetSubObject(nSubObj);
            if(pSubObj->pStatObj)
            {
              rmi.pMesh = pSubObj->pStatObj->GetRenderMesh();
              rmi.pMat = pSubObj->pStatObj->GetMaterial();
              rmi.mat = objMat*pSubObj->tm;
              if(rmi.pMesh)
              {
                AABB transAABB = AABB::CreateTransformedAABB(rmi.mat,pSubObj->pStatObj->GetAABB());
                if(Overlap::AABB_AABB(decalAABB,transAABB))
                  lstRMI.Add(rmi);
              }
            }
          }
        }
      }
      else if(pDecalOwner->GetRenderNodeType() == eERType_VoxelObject)
      {
        SRenderMeshInfoInput rmi;
        CVoxelObject * pVoxObj = (CVoxelObject *)pDecalOwner;
        rmi.pMesh = pVoxObj->GetRenderMesh(0);
        if(rmi.pMesh && rmi.pMesh->GetChunks())
        {
          rmi.pMat = rmi.pMesh->GetMaterial();
          rmi.mat = pVoxObj->GetMatrix();
          AABB transAABB = pDecalOwner->GetBBox();
          if(Overlap::AABB_AABB(decalAABB,transAABB))
            lstRMI.Add(rmi);
        }
      }
		}

		if(!lstRMI.Count())
			return false;

		CRenderMeshMerger::SDecalClipInfo DecalClipInfo;
		DecalClipInfo.vPos = DecalInfo.vPos;
		DecalClipInfo.fRadius = DecalInfo.fSize;
		DecalClipInfo.vProjDir = DecalInfo.vNormal;

		PodArray<SRenderMeshInfoOutput> outRenderMeshes;
		CRenderMeshMerger Merger;
		CRenderMeshMerger::SMergeInfo info;
		info.sMeshName = "MergedDecal";
		info.sMeshType = "MergedDecal";
		info.pDecalClipInfo = &DecalClipInfo;
		newDecal.m_pRenderMesh = Merger.MergeRenderMeshes(lstRMI.GetElements(), lstRMI.Count(), outRenderMeshes,info);

		if(!newDecal.m_pRenderMesh)
			return false; // no geometry found

		assert(newDecal.m_pRenderMesh->GetChunks()->Count()==1);

		newDecal.m_pRenderMesh->SetMaterial( newDecal.m_pMaterial );
	}
	else if(DecalInfo.ownerInfo.pRenderNode && 
		(DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Unknown || DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation) && 
		DecalInfo.ownerInfo.nRenderNodeSlotId>=0)
	{ 
		newDecal.m_eDecalType = eDecalType_OS_SimpleQuad;

		Matrix34 objMat;

		// transform decal from world space into entity space
		IStatObj * pEntObject = DecalInfo.ownerInfo.GetOwner(objMat);
		if(!pEntObject)
			return false;
		assert(pEntObject);
		objMat.Invert();

		if( useDefinedUpRight )
		{
			userDefinedRight = objMat.TransformVector( userDefinedRight ).GetNormalized();
			userDefinedUp = objMat.TransformVector( userDefinedUp ).GetNormalized();
			assert( fabsf( DecalInfo.vNormal.Dot( -DecalInfo.vHitDirection.GetNormalized() ) - 1.0f ) < 1e-4f );
		}

		DecalInfo.vNormal = objMat.TransformVector(DecalInfo.vNormal).GetNormalized();
		DecalInfo.vPos = objMat.TransformPoint(DecalInfo.vPos);

		// find object scale
		if(DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType()==eERType_Vegetation)
      fObjScale = ((CVegetation*)(DecalInfo.ownerInfo.pRenderNode))->GetScale();
		else
		{
			Vec3 vTest(0,0,1.f);
			vTest = objMat.TransformVector(vTest);
			fObjScale = 1.f/vTest.len();
		}

    DecalInfo.fSize/=fObjScale;
	}
	else 
	{
		if(!DecalInfo.preventDecalOnGround && DecalInfo.fSize>(GetCVars()->e_decals_wrap_around_min_size*2.f) && !DecalInfo.ownerInfo.pRenderNode && 
			 (DecalInfo.vPos.z-pTerrain->GetZApr(DecalInfo.vPos.x,DecalInfo.vPos.y))<DecalInfo.fSize)
		{
			newDecal.m_eDecalType = eDecalType_WS_OnTheGround;

			int nUnitSize = CTerrain::GetHeightMapUnitSize();
			int x1 = int(DecalInfo.vPos.x-DecalInfo.fSize)/nUnitSize*nUnitSize-nUnitSize;
			int x2 = int(DecalInfo.vPos.x+DecalInfo.fSize)/nUnitSize*nUnitSize+nUnitSize;
			int y1 = int(DecalInfo.vPos.y-DecalInfo.fSize)/nUnitSize*nUnitSize-nUnitSize;
			int y2 = int(DecalInfo.vPos.y+DecalInfo.fSize)/nUnitSize*nUnitSize+nUnitSize;

			for(int x=x1; x<=x2; x+=CTerrain::GetHeightMapUnitSize())
			for(int y=y1; y<=y2; y+=CTerrain::GetHeightMapUnitSize())
				if(pTerrain->GetHole(x,y))
					return false;
		}
		else
			newDecal.m_eDecalType = eDecalType_WS_SimpleQuad;
		
		DecalInfo.ownerInfo.pRenderNode = NULL;
	}

  // spawn
	if( !useDefinedUpRight )
	{
    if(DecalInfo.vNormal.Dot(Vec3(0,0,1))>0.999f)
    { // floor
      newDecal.m_vRight = Vec3(0,1,0);
      newDecal.m_vUp    = Vec3(-1,0,0);
    }
		else if(DecalInfo.vNormal.Dot(Vec3(0,0,-1))>0.999f)
		{ // ceil
			newDecal.m_vRight = Vec3(1,0,0);
			newDecal.m_vUp    = Vec3(0,-1,0);
		}
		else if(!DecalInfo.vNormal.IsZero())
		{
			newDecal.m_vRight = DecalInfo.vNormal.Cross(Vec3(0,0,1));
			newDecal.m_vRight.Normalize();
			newDecal.m_vUp    = DecalInfo.vNormal.Cross(newDecal.m_vRight);
			newDecal.m_vUp.Normalize();
		}

		// rotate vectors
		if(!DecalInfo.vNormal.IsZero())
		{
			AngleAxis rotation(DecalInfo.fAngle,DecalInfo.vNormal);
			newDecal.m_vRight = rotation*newDecal.m_vRight;
			newDecal.m_vUp		= rotation*newDecal.m_vUp;
		}
	}
	else
	{
		newDecal.m_vRight = userDefinedRight;
		newDecal.m_vUp = userDefinedUp;
	}

	newDecal.m_vFront		= DecalInfo.vNormal;

	newDecal.m_vPos = DecalInfo.vPos;
	newDecal.m_vPos += DecalInfo.vNormal * 0.001f / fObjScale;

	newDecal.m_fSize = DecalInfo.fSize;
  newDecal.m_fLifeTime = DecalInfo.fLifeTime*GetCVars()->e_decals_life_time_scale;
  assert(!DecalInfo.pIStatObj); // not used -> not supported
	newDecal.m_vAmbient = Get3DEngine()->GetAmbientColorFromPosition(newDecal.m_vWSPos);

	newDecal.m_ownerInfo.pRenderNode = DecalInfo.ownerInfo.pRenderNode;
	if (DecalInfo.ownerInfo.pRenderNode)
		DecalInfo.ownerInfo.pRenderNode->m_nInternalFlags |= IRenderNode::DECAL_OWNER;

	newDecal.m_fGrowTime = DecalInfo.fGrowTime;
	newDecal.m_fLifeBeginTime = GetTimer()->GetCurrTime();

  if(DecalInfo.pIStatObj && !pCallerManagedDecal)
  {
		//assert(!"Geometry decals neede to be re-debugged");
    DecalInfo.pIStatObj->AddRef();
  }

	if( !pCallerManagedDecal )
	{
		m_arrbActiveDecals[ m_nCurDecal ] = true;
		++m_nCurDecal;
	}

#ifdef _DEBUG
	if( newDecal.m_ownerInfo.pRenderNode )
	{
		int n = sizeof( newDecal.m_decalOwnerEntityClassName );
		strncpy( newDecal.m_decalOwnerEntityClassName, newDecal.m_ownerInfo.pRenderNode->GetEntityClassName(), n );
		newDecal.m_decalOwnerEntityClassName[n-1] = '\0';		
		n = sizeof( newDecal.m_decalOwnerName );
		strncpy( newDecal.m_decalOwnerName, newDecal.m_ownerInfo.pRenderNode->GetName(), n );
		newDecal.m_decalOwnerEntityClassName[n-1] = '\0';		
		newDecal.m_decalOwnerType = newDecal.m_ownerInfo.pRenderNode->GetRenderNodeType();
	}
	else
	{
		newDecal.m_decalOwnerEntityClassName[0] = '\0';
		newDecal.m_decalOwnerName[0] = '\0';
		newDecal.m_decalOwnerType = eERType_Unknown;
	}
#endif

	return true;
}

void CDecalManager::Update( const float fFrameTime )
{
	int i,nGeomChecks;
  for(i=nGeomChecks=0; i<DECAL_COUNT; i++)
	  if(m_arrbActiveDecals[i])
			nGeomChecks += m_arrDecals[i].Update(m_arrbActiveDecals[i],fFrameTime);

	if (nGeomChecks) for(i=0; i<DECAL_COUNT; i++)	if(m_arrbActiveDecals[i] && m_arrDecals[i].m_ownerInfo.pRenderNode)
		m_arrDecals[i].m_ownerInfo.pRenderNode->m_nInternalFlags &= ~IRenderNode::UPDATE_DECALS;
}


void CDecalManager::Render()
{
	FUNCTION_PROFILER_3DENGINE;

  if(!GetCVars()->e_decals || !GetObjManager())
    return;

	float fCurrTime = GetTimer()->GetCurrTime();
	float fZoom = GetObjManager()->m_fZoomFactor;
	float fWaterLevel = m_p3DEngine->GetWaterLevel();

	// draw
  for(int i=0; i<DECAL_COUNT; i++)
  if(m_arrbActiveDecals[i])
  {
		CDecal * pDecal = &m_arrDecals[i];
    pDecal->m_vWSPos = pDecal->GetWorldPosition();
    float fDist = GetCamera().GetPosition().GetDistance(pDecal->m_vWSPos)*fZoom;
    float fMaxViewDist = pDecal->m_fWSSize*ENTITY_DECAL_DIST_FACTOR*3.0f;
		if(fDist < fMaxViewDist)
			if(GetCamera().IsSphereVisible_F(Sphere(pDecal->m_vWSPos, pDecal->m_fWSSize)))
		{
      bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos,GetCamera().GetPosition(),fWaterLevel);
			if( pDecal->m_pMaterial )
			{
				if((GetMainFrameID()&15) == (i&15)) // ambient may change any time
					pDecal->m_vAmbient = Get3DEngine()->GetAmbientColorFromPosition(pDecal->m_vWSPos);

        // TODO: take entity orientation into account
        Vec3 vSize(pDecal->m_fWSSize,pDecal->m_fWSSize,pDecal->m_fWSSize);
        AABB aabb(pDecal->m_vWSPos-vSize,pDecal->m_vWSPos+vSize);
        
        uint nDLMask = 0;
        IRenderNode * pRN = pDecal->m_ownerInfo.pRenderNode;
        if(COctreeNode * pNode = (COctreeNode*)(pRN ? pRN->m_pOcNode : NULL))
          nDLMask = Get3DEngine()->BuildLightMask( aabb, pNode->GetAffectingLights(), (CVisArea*)pRN->GetEntityVisArea(), (pRN->GetRndFlags()&ERF_OUTDOORONLY)!=0 );
        else
          nDLMask = Get3DEngine()->BuildLightMask(aabb);

        float fDistFading = SATURATE((1.f - fDist/fMaxViewDist)*DIST_FADING_FACTOR);
				pDecal->Render( fCurrTime, bAfterWater, nDLMask, fDistFading );

				if(GetCVars()->e_decals>1)
				{
          Vec3 vCenter = aabb.GetCenter();
          AABB aabbCenter(vCenter-vSize*0.05f, vCenter+vSize*0.05f);

          DrawBBox(aabb);
          DrawBBox(aabbCenter, Col_Yellow);

          if(pDecal->m_pRenderMesh)
          {
            pDecal->m_pRenderMesh->GetBBox(aabb.min,aabb.max);
            DrawBBox(aabb, Col_Red);
          }
				}
			}
		}
  }
}

//////////////////////////////////////////////////////////////////////////
void CDecalManager::OnEntityDeleted(IRenderNode * pRenderNode)
{
	// remove decals of this entity
	for(int i=0; i<DECAL_COUNT; i++)
	{
		if(m_arrbActiveDecals[i])
		{
			if(m_arrDecals[i].m_ownerInfo.pRenderNode == pRenderNode)
			{
				m_arrbActiveDecals[i] = false;
				m_arrDecals[i].FreeRenderData();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDecalManager::OnRenderMeshDeleted(IRenderMesh * pRenderMesh)
{
	// remove decals of this entity
	for(int i=0; i<DECAL_COUNT; i++)
	{
		if(m_arrbActiveDecals[i])
		{
			if(	
				(m_arrDecals[i].m_ownerInfo.pRenderNode && (
				m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(0) == pRenderMesh ||
				m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(1) == pRenderMesh ||
				m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(2) == pRenderMesh)) 
				||
				(m_arrDecals[i].m_pRenderMesh && m_arrDecals[i].m_pRenderMesh->GetVertexContainer() == pRenderMesh) 
				)
			{
				m_arrbActiveDecals[i] = false;
				m_arrDecals[i].FreeRenderData();
//				PrintMessage("CDecalManager::OnRenderMeshDeleted succseed");
			}
		}
	}
}

void CDecalManager::FillBigDecalIndices(IRenderMesh * pRM, Vec3 vPos, float fRadius, Vec3 vProjDir, PodArray<ushort> * plstIndices, IMaterial * pMat, AABB & meshBBox)
{
	FUNCTION_PROFILER_3DENGINE;

  // get position offset and stride
	int nPosStride = 0;
	byte * pPos  = pRM->GetStridedPosPtr(nPosStride);

  ushort *pInds = pRM->GetIndices(0);
  int nInds = pRM->GetSysIndicesCount();

	assert(nInds%3 == 0);

	plstIndices->Clear();

	if(!vProjDir.IsZero())
		vProjDir.Normalize();

  if(!pMat)
    pMat = pRM->GetMaterial();

  if(!pMat)
    return;

  // render tris
  PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();
  for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
  {
    CRenderChunk * pChunk = pChunks->Get(nChunkId);
    if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
      continue;

    const SShaderItem &shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);

    if(!shaderItem.m_pShader)
      continue;

    if(shaderItem.m_pShader->GetFlags2()&EF2_NODRAW)
      continue;

    if(shaderItem.m_pShader->GetFlags()&EF_DECAL)
      continue;

/*
    if (!shaderItem.IsZWrite())
      continue;
    ECull nCullMode = shaderItem.m_pShader->GetCull();
*/

    int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
    for(int i=pChunk->nFirstIndexId; i<nLastIndexId; i+=3)
    {
      assert(pInds[i+0] < pChunk->nFirstVertId+pChunk->nNumVerts);
      assert(pInds[i+1] < pChunk->nFirstVertId+pChunk->nNumVerts);
      assert(pInds[i+2] < pChunk->nFirstVertId+pChunk->nNumVerts);
      assert(pInds[i+0] >= pChunk->nFirstVertId);
      assert(pInds[i+1] >= pChunk->nFirstVertId);
      assert(pInds[i+2] >= pChunk->nFirstVertId);

		  // get tri vertices
		  Vec3 v0 = *((Vec3*)&pPos[nPosStride*pInds[i+0]]);
		  Vec3 v1 = *((Vec3*)&pPos[nPosStride*pInds[i+1]]);
		  Vec3 v2 = *((Vec3*)&pPos[nPosStride*pInds[i+2]]);

		  // find bbox
      AABB triBBox;
		  triBBox.min = triBBox.max = v0; 
      triBBox.Add(v1);
      triBBox.Add(v2);

		  if(vProjDir.IsZero())
		  { // explo mode
			  // get dir to triangle
  //			Vec3 vCenter = (v0+v1+v2)*0.33333f;
  			
			  // test the face
			  float fDot0 = (vPos-v0).Dot((v0-v1).Cross(v2-v1));
			  if(fDot0)
				  continue;
		  }
		  else
		  {
			  // get triangle normal
			  Vec3 vNormal = (v1-v0).Cross(v2-v0);
			  vNormal.NormalizeSafe();

			  // test the face
			  if(vNormal.Dot(vProjDir)<0)
				  continue;
		  }

		  if(Overlap::Sphere_AABB(Sphere(vPos, fRadius), triBBox))
		  {
			  plstIndices->Add(pInds[i+0]);
			  plstIndices->Add(pInds[i+1]);
			  plstIndices->Add(pInds[i+2]);

        meshBBox.Add(triBBox);
		  }
    }
	}
}

IRenderMesh * CDecalManager::MakeBigDecalRenderMesh(IRenderMesh * pSourceRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, IMaterial* pDecalMat, IMaterial* pSrcMat)
{
  if(!pSourceRenderMesh || pSourceRenderMesh->GetVertCount()==0)
    return 0;

	// make indices of this decal
	PodArray<ushort> lstIndices;		
	
  AABB meshBBox(vPos,vPos);

	if(pSourceRenderMesh && pSourceRenderMesh->GetSysVertBuffer() && pSourceRenderMesh->GetVertCount())
		FillBigDecalIndices(pSourceRenderMesh, vPos, fRadius, vProjDir, &lstIndices, pSrcMat, meshBBox);

	if(!lstIndices.Count())
		return 0;

	// make fake vert buffer with one vertex // todo: remove this
  PodArray<struct_VERTEX_FORMAT_P3F_COL4UB> EmptyVertBuffer;
  EmptyVertBuffer.Add(struct_VERTEX_FORMAT_P3F_COL4UB());

	IRenderMesh* pRenderMesh( 0 );
	pRenderMesh = GetRenderer()->CreateRenderMeshInitialized( EmptyVertBuffer.GetElements(), EmptyVertBuffer.Count(), VERTEX_FORMAT_P3F_COL4UB,
		lstIndices.GetElements(), lstIndices.Count(), R_PRIMV_TRIANGLES, "BigDecalOnStatObj","BigDecal", eBT_Static, 1 );
	pRenderMesh->SetVertexContainer(pSourceRenderMesh);
	pRenderMesh->SetChunk( pDecalMat, 0, pSourceRenderMesh->GetVertCount(), 0, lstIndices.Count() );
	pRenderMesh->SetMaterial( pDecalMat );
  pRenderMesh->SetBBox(meshBBox.min, meshBBox.max);

  return pRenderMesh;
}

/*
IRenderMesh * CDecalManager::MakeBigDecalRenderMesh(IRenderMesh * pSourceRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, int nTexID)
{
if(!pSourceRenderMesh || pSourceRenderMesh->GetSysVertCount()==0)
return 0;

// make indices of this decal
PodArray<ushort> lstIndices;		

if(pSourceRenderMesh && pSourceRenderMesh->GetSysVertBuffer() && pSourceRenderMesh->GetSysVertCount())
FillBigDecalIndices(pSourceRenderMesh, vPos, fRadius, vProjDir, &lstIndices);

if(!lstIndices.Count())
return 0;

// make fake vert buffer with one vertex // todo: remove this
PodArray<struct_VERTEX_FORMAT_P3F_N> VertBuffer;
VertBuffer.PreAllocate(pSourceRenderMesh->GetSysVertCount());
int nPosStride=0, nNormStride=0;
void * pPos = pSourceRenderMesh->GetPosPtr(nPosStride);
void * pNorm = pSourceRenderMesh->GetPosPtr(nNormStride);
for(int i=0; i<pSourceRenderMesh->GetSysVertCount(); i++)
{
struct_VERTEX_FORMAT_P3F_N vert;
vert.xyz = *(Vec3*)((uchar*)pPos+nPosStride*i);
vert.normal = *(Vec3*)((uchar*)pNorm+nNormStride*i);
vert.normal = vert.normal*5;
VertBuffer.Add(vert);
}

IRenderMesh * pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
VertBuffer.GetElements(), VertBuffer.Count(), VERTEX_FORMAT_P3F_N,
lstIndices.GetElements(), lstIndices.Count(),
R_PRIMV_TRIANGLES, "BigDecal", eBT_Static, 1, nTexID);

pRenderMesh->SetChunk(m_pShader_Decal_VP, 0, pSourceRenderMesh->GetSysVertCount(), 0, lstIndices.Count());
pRenderMesh->SetMaterial(m_pShader_Decal_VP,nTexID);

return pRenderMesh;
}
*/
void CDecalManager::GetMemoryUsage(ICrySizer*pSizer)
{
	pSizer->Add (*this);
}

void CDecalManager::DeleteDecalsInRange( AABB * pAreaBox, IRenderNode * pEntity )
{
  if(GetCVars()->e_decals>1 && pAreaBox)
    DrawBBox(*pAreaBox);

	for(int i=0; i<DECAL_COUNT; i++)
  {
		if(!m_arrbActiveDecals[i])
      continue;

    if(pEntity && (pEntity != m_arrDecals[i].m_ownerInfo.pRenderNode))
      continue;

    if(pAreaBox)
    {
      Vec3 vPos = m_arrDecals[i].GetWorldPosition();
      Vec3 vSize(m_arrDecals[i].m_fWSSize, m_arrDecals[i].m_fWSSize, m_arrDecals[i].m_fWSSize);
      AABB decalBox(vPos-vSize, vPos+vSize);
      if( !Overlap::AABB_AABB(*pAreaBox, decalBox) )
        continue;
    }

    if(m_arrDecals[i].m_eDecalType != eDecalType_WS_OnTheGround)
		  m_arrbActiveDecals[i] = false;

		m_arrDecals[i].FreeRenderData();
  }
}
/*
struct CDebugLine
{
	Vec3 v0,v1;
	int nFrameId;
};

PodArray<CDebugLine> lstDebugLines;

void AddDebugLine(Vec3 v0, Vec3 v1, int nFrameId)
{
	CDebugLine l;
	l.v0 = v0;
	l.v1 = v1;
	l.nFrameId = nFrameId;
	lstDebugLines.Add(l);
}

void DrawDebugLines(IRenderer*pRenderer)
{
	for(int i=0; i<lstDebugLines.Count(); i++)
	{
		pRenderer->Draw3dPrim(lstDebugLines[i].v0,lstDebugLines[i].v1,DPRIM_LINE);
		if(pRenderer->GetFrameID()>lstDebugLines[i].nFrameId+1000)
		{
			lstDebugLines.Delete(i);
			i--;
		}
	}
}*/

void CDecalManager::Serialize( TSerialize ser )
{
	ser.BeginGroup("StaticDecals");

	if(ser.IsReading())
		Reset();

	uint32 dwDecalCount=0;

  for(uint32 dwI=0; dwI<DECAL_COUNT; dwI++)
	  if(m_arrbActiveDecals[dwI])
			dwDecalCount++;

	ser.Value("DecalCount",dwDecalCount);

	if(ser.IsWriting())
	{
	  for(uint32 dwI=0; dwI<DECAL_COUNT; dwI++)
			if(m_arrbActiveDecals[dwI])
			{
				CDecal &ref = m_arrDecals[dwI];

				ser.BeginGroup("Decal");
				ser.Value("Pos",ref.m_vPos);
				ser.Value("Right",ref.m_vRight);
				ser.Value("Up",ref.m_vUp);
				ser.Value("Front",ref.m_vFront);
				ser.Value("Size",ref.m_fSize);
				ser.Value("WSPos",ref.m_vWSPos);
				ser.Value("WSSize",ref.m_fWSSize);
				ser.Value("fLifeTime",ref.m_fLifeTime);

				// serialize material, handle legacy decals with textureID converted to material created at runtime
				string matName( "" );
				if( ref.m_pMaterial && ref.m_pMaterial->GetName() )
					matName = ref.m_pMaterial->GetName();
				ser.Value( "MatName", matName );
				ser.Value( "IsTempMat", ref.m_isTempMat );

//			TODO:  IStatObj *		m_pStatObj;												// only if real 3d object is used as decal ()
				ser.Value("vAmbient",ref.m_vAmbient);
//			TODO:  IRenderNode * m_pDecalOwner;										// transformation parent object (0 means parent is the world)
        ser.Value("nRenderNodeSlotId",ref.m_ownerInfo.nRenderNodeSlotId);
        ser.Value("nRenderNodeSlotSubObjectId",ref.m_ownerInfo.nRenderNodeSlotSubObjectId);
				
				int nDecalType = (int)ref.m_eDecalType;
				ser.Value("eDecalType", nDecalType);

				ser.Value("fGrowTime",ref.m_fGrowTime);
				ser.Value("fLifeBeginTime",ref.m_fLifeBeginTime);

				bool bBigDecalUsed = ref.IsBigDecalUsed();

				ser.Value("bBigDecal",bBigDecalUsed);

				// m_arrBigDecalRMs[] will be created on the fly so no need load/save it

				if(bBigDecalUsed)
				{
					for(uint32 dwI=0;dwI<sizeof(ref.m_arrBigDecalRMCustomData)/sizeof(ref.m_arrBigDecalRMCustomData[0]);++dwI)
					{
						char szName[16];

						sprintf(szName,"BDCD%d",dwI);
						ser.Value(szName,ref.m_arrBigDecalRMCustomData[dwI]);
					}
				}
				ser.EndGroup();
			}
	}
	else
	if(ser.IsReading())
	{
		m_nCurDecal=0;

		for(uint32 dwI=0; dwI<dwDecalCount; dwI++)
			if(m_nCurDecal<DECAL_COUNT)
			{
				CDecal &ref = m_arrDecals[m_nCurDecal];

				ref.FreeRenderData();

				ser.BeginGroup("Decal");
				ser.Value("Pos",ref.m_vPos);
				ser.Value("Right",ref.m_vRight);
				ser.Value("Up",ref.m_vUp);
				ser.Value("Front",ref.m_vFront);
				ser.Value("Size",ref.m_fSize);
				ser.Value("WSPos",ref.m_vWSPos);
				ser.Value("WSSize",ref.m_fWSSize);
				ser.Value("fLifeTime",ref.m_fLifeTime);
				
				// serialize material, handle legacy decals with textureID converted to material created at runtime
				string matName( "" );
				ser.Value( "MatName", matName );
				bool isTempMat( false );
				ser.Value( "IsTempMat", isTempMat );

				ref.m_pMaterial = 0;
				ref.m_isTempMat = false;
				if( isTempMat && !matName.empty() )
				{
					ref.m_pMaterial = GetMaterialForDecalTexture( matName.c_str() );
					ref.m_isTempMat = ref.m_pMaterial ? true : false;
				}
				else if( !matName.empty() )
				{
					ref.m_pMaterial = GetMatMan()->LoadMaterial( matName.c_str(), false,true );
					if( !ref.m_pMaterial )
						Warning( "Decal material \"%s\" not found!\n", matName.c_str() );
				}

//			TODO:  IStatObj *		m_pStatObj;												// only if real 3d object is used as decal ()
				ser.Value("vAmbient",ref.m_vAmbient);
//			TODO:  IRenderNode * m_pDecalOwner;										// transformation parent object (0 means parent is the world)
        ser.Value("nRenderNodeSlotId",ref.m_ownerInfo.nRenderNodeSlotId);
        ser.Value("nRenderNodeSlotSubObjectId",ref.m_ownerInfo.nRenderNodeSlotSubObjectId);
				
				int nDecalType = eDecalType_Undefined;
				ser.Value("eDecalType",nDecalType);
				ref.m_eDecalType = (EDecal_Type)nDecalType;
				
				ser.Value("fGrowTime",ref.m_fGrowTime);
				ser.Value("fLifeBeginTime",ref.m_fLifeBeginTime);

				// no need to to store m_arrBigDecalRMs[] as it becomes recreated

				bool bBigDecalsAreaUsed=false;

				ser.Value("bBigDecals",bBigDecalsAreaUsed);

				if(bBigDecalsAreaUsed)
				for(uint32 dwI=0;dwI<sizeof(ref.m_arrBigDecalRMCustomData)/sizeof(ref.m_arrBigDecalRMCustomData[0]);++dwI)
				{
					char szName[16];

					sprintf(szName,"BDCD%d",dwI);
					ser.Value(szName,ref.m_arrBigDecalRMCustomData[dwI]);
				}

				// m_arrBigDecalRMs[] will be created on the fly so no need load/save it

				m_arrbActiveDecals[m_nCurDecal] = bool(nDecalType != eDecalType_Undefined);

				++m_nCurDecal;
				ser.EndGroup();
			}
	}

	ser.EndGroup();
}

IMaterial* CDecalManager::GetMaterialForDecalTexture( const char* pTextureName )
{
	if( !pTextureName || *pTextureName == 0 )
		return 0;

	IMaterialManager* pMatMan( GetMatMan() );
	IMaterial* pMat( pMatMan->FindMaterial( pTextureName ) );
	if( pMat )
		return pMat;

	IMaterial* pMatSrc( pMatMan->LoadMaterial( "Materials/Decals/Default", false,true ) );
	if( pMatSrc )
	{
		IMaterial* pMatDst( pMatMan->CreateMaterial( pTextureName, pMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE ) );
		if( pMatDst )
		{
			SShaderItem& si( pMatSrc->GetShaderItem() );

			SInputShaderResources isr( si.m_pShaderResources );
			isr.m_Textures[ EFTT_DIFFUSE ].m_Name = pTextureName;

			SShaderItem siDst( GetRenderer()->EF_LoadShaderItem( si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask() ) );
			pMatDst->AssignShaderItem( siDst );

			return pMatDst;
		}
	}	

	return 0;
}

IStatObj * SDecalOwnerInfo::GetOwner(Matrix34 & objMat)
{
	if(!pRenderNode)
		return NULL;

	IStatObj * pStatObj = NULL;
	if(pStatObj = pRenderNode->GetEntityStatObj(nRenderNodeSlotId, nRenderNodeSlotSubObjectId, &objMat, true))
	{
    if(nRenderNodeSlotSubObjectId>=0 && nRenderNodeSlotSubObjectId<pStatObj->GetSubObjectCount())
    {
      IStatObj::SSubObject * pSubObj = pStatObj->GetSubObject(nRenderNodeSlotSubObjectId);
      pStatObj = pSubObj->pStatObj;
      objMat = objMat*pSubObj->tm;
    }
	}
	else if(ICharacterInstance * pChar = pRenderNode->GetEntityCharacter(nRenderNodeSlotId, &objMat))
	{
		if(nRenderNodeSlotSubObjectId>=0)
		{
			pStatObj = pChar->GetISkeletonPose()->GetStatObjOnJoint(nRenderNodeSlotSubObjectId);
			const QuatT q = pChar->GetISkeletonPose()->GetAbsJointByID(nRenderNodeSlotSubObjectId);
			objMat = objMat*Matrix34(q);
		}
	}

	if(pStatObj && (pStatObj->GetFlags()&STATIC_OBJECT_HIDDEN))
    return NULL;

  if(pStatObj)
    if(int nMinLod = ((CStatObj*)pStatObj)->GetMinUsableLod())
      if(IStatObj *pLodObj = pStatObj->GetLodObject(nMinLod))
        pStatObj = pLodObj;

	return pStatObj;
}
