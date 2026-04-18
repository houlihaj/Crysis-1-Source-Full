#include "StdAfx.h"
#include "CullBuffer.h"
#include "RoadRenderNode.h"
#include "VoxMan.h"
#include "Brush.h"
#include "RenderMeshMerger.h"
#include "DecalManager.h"
#include "Material.h"
#include "VisAreas.h"
#include "ICryAnimation.h"
#include "LightEntity.h"

const float fNodeMinSize = 16.f;
const float fObjectToNodeSizeRatio = 1.f/8.f;
const float fMinShadowCasterViewDist = 8.f;
float COctreeNode::m_fAreaMergingTimeLimit = 0;

PodArray<COctreeNode*> COctreeNode::m_arrEmptyNodes;

#ifndef PI
#define PI 3.14159f
#endif

COctreeNode::COctreeNode(const AABB & box, CVisArea * pVisArea, COctreeNode * pParent)
{ 
	memset(this, 0, sizeof(*this)); 
	m_nodeBox = box; 
  m_vNodeCenter = m_nodeBox.GetCenter();
  m_fNodeRadius = m_nodeBox.GetRadius();
	m_objectsBox.min = m_nodeBox.max;
	m_objectsBox.max = m_nodeBox.min;
	m_pTerrainNode = GetTerrain() ? GetTerrain()->FindMinNodeContainingBox(m_nodeBox) : NULL;
	m_pVisArea = pVisArea;
  m_pParent = pParent;

//	for(int n=0; n<2 && m_pTerrainNode && m_pTerrainNode->m_pParent; n++)
	//	m_pTerrainNode = m_pTerrainNode->m_pParent;
	m_vSunDirReady.Set(0,0,-1);
  m_pCompiledInstances = NULL;
}

COctreeNode::~COctreeNode()
{
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode, * pNext; pObj; pObj = pNext)
  {
    pNext = pObj->m_pNext;
		EERType t = pObj->GetRenderNodeType();
		if(t != eERType_Unknown)
		  pObj->ReleaseNode();
		else
			Get3DEngine()->UnRegisterEntity(pObj);
	}

	assert(!m_lstObjects.m_pFirstNode);

	for(int i=0; i<8; i++) 
		delete m_arrChilds[i];

	FreeAreaBrushes();

  m_arrEmptyNodes.Delete(this);

  SAFE_DELETE(m_pCompiledInstances);
}

void COctreeNode::InsertObject(IRenderNode * pObj, const AABB & objBox, const float fObjRadius, const Vec3 & vObjCenter)
{
	FUNCTION_PROFILER_3DENGINE;

	// parent bbox includes all childs
	m_objectsBox.Add(objBox);

	m_fObjectsMaxViewDist = max(m_fObjectsMaxViewDist, pObj->m_fWSMaxViewDist);

	if (pObj->GetRndFlags() & ERF_GOOD_OCCLUDER)
		m_bHasOccluders = true;

	if(pObj->GetRndFlags()&ERF_CASTSHADOWMAPS)
		m_bHasShadowCasters = true;

  EERType eType = pObj->GetRenderNodeType();

	if(eType == eERType_Light)
		m_bHasLights = true;

  if(eType == eERType_VoxelObject)
    m_bHasVoxels = true;

	if(m_nodeBox.max.x-m_nodeBox.min.x > fNodeMinSize && 
    eType != eERType_VoxelObject && eType != eERType_RoadObject_NEW) // store voxels and roads in root
	{
		if(fObjRadius < m_fNodeRadius*fObjectToNodeSizeRatio)
      if(pObj->m_fWSMaxViewDist < m_fNodeRadius*GetCVars()->e_view_dist_ratio_vegetation)
		{
			int nChildId = 
				((vObjCenter.x > m_vNodeCenter.x) ? 4 : 0) |
				((vObjCenter.y > m_vNodeCenter.y) ? 2 : 0) |
				((vObjCenter.z > m_vNodeCenter.z) ? 1 : 0);

			if(!m_arrChilds[nChildId])
				m_arrChilds[nChildId] = new COctreeNode(GetChildBBox(nChildId), m_pVisArea, this);

			m_arrChilds[nChildId]->InsertObject(pObj, objBox, fObjRadius, vObjCenter);
			return;
		}
	}

	LinkObject(pObj);

	pObj->m_pOcNode = this;

  if(eType != eERType_Light)
  	m_bCompiled = false;

	if(eType == eERType_Brush || eType == eERType_Vegetation)
		m_bAreaMeshReady = false;
}

void COctreeNode::LinkObject(IRenderNode * pObj)
{
  assert(!pObj->m_pNext && !pObj->m_pPrev);

  assert(!m_lstObjects.m_pFirstNode || !m_lstObjects.m_pFirstNode->m_pPrev);
  assert(!m_lstObjects.m_pLastNode || !m_lstObjects.m_pLastNode->m_pNext);

  if(pObj->m_dwRndFlags & ERF_CASTSHADOWMAPS)
    m_lstObjects.insertBeginning(pObj);
  else
    m_lstObjects.insertEnd(pObj);

  assert(!m_lstObjects.m_pFirstNode || !m_lstObjects.m_pFirstNode->m_pPrev);
  assert(!m_lstObjects.m_pLastNode || !m_lstObjects.m_pLastNode->m_pNext);
}

void COctreeNode::UnlinkObject(IRenderNode * pObj)
{
  assert(!m_lstObjects.m_pFirstNode || !m_lstObjects.m_pFirstNode->m_pPrev);
  assert(!m_lstObjects.m_pLastNode || !m_lstObjects.m_pLastNode->m_pNext);

  if(pObj->m_pNext || pObj->m_pPrev || pObj == m_lstObjects.m_pLastNode || pObj == m_lstObjects.m_pFirstNode)
    m_lstObjects.remove(pObj);

  assert(!m_lstObjects.m_pFirstNode || !m_lstObjects.m_pFirstNode->m_pPrev);
  assert(!m_lstObjects.m_pLastNode || !m_lstObjects.m_pLastNode->m_pNext);
}

bool COctreeNode::DeleteObject(IRenderNode * pObj)
{
	FUNCTION_PROFILER_3DENGINE;

	if(pObj->m_pOcNode && pObj->m_pOcNode != this)
		return ((COctreeNode*)(pObj->m_pOcNode))->DeleteObject(pObj);

	UnlinkObject(pObj);

  m_lstOccluders.Delete(pObj);
	m_lstMergedObjects.Delete(pObj);

  SAFE_DELETE(m_pCompiledInstances);

	pObj->m_pOcNode = NULL;

	if(pObj->GetRenderNodeType() == eERType_Brush || pObj->GetRenderNodeType() == eERType_Vegetation)
		m_bAreaMeshReady = false;

  if( IsEmpty() && m_arrEmptyNodes.Find(this)<0 )
      m_arrEmptyNodes.Add(this);

  return true;
}

bool COctreeNode::Render(const CCamera & rCam, CCullBuffer & rCB, int nRenderMask, const Vec3 & vAmbColor)
{
	FUNCTION_PROFILER_3DENGINE;
/*
  if(m_nodeBox.GetSize().x == fNodeMinSize)
  {
    if(GetCVars()->e_terrain_draw_this_sector_only)
    {
      if(
        GetCamera().GetPosition().x > m_nodeBox.max.x || 
        GetCamera().GetPosition().x < m_nodeBox.min.x ||
        GetCamera().GetPosition().y > m_nodeBox.max.y || 
        GetCamera().GetPosition().y < m_nodeBox.min.y )
        return false;
      else
        int test=0;
    }
  }
*/
	if(nRenderMask & OCTREENODE_RENDER_FLAG_OCCLUDERS)
		if(!m_bHasOccluders)
			return false;

	if(m_nOccludedFrameId == GetFrameID())
		if(&rCB == Get3DEngine()->GetCoverageBuffer())
			return false;

	bool bNodeCompletelyInFrustum = false, bNodeMostlyInFrustum = false;
	if(!rCam.IsAABBVisible_EHM( m_objectsBox, &bNodeCompletelyInFrustum ))
		return false;

  bNodeMostlyInFrustum = bNodeCompletelyInFrustum;

  if(!bNodeMostlyInFrustum && m_pCompiledInstances)
  {
    Vec3 vSize = m_aabbCompiledInstances.GetSize()*.4f;
    AABB croppedBox(m_aabbCompiledInstances.min + vSize, m_aabbCompiledInstances.max - vSize);
    rCam.IsAABBVisible_EHM( croppedBox, &bNodeMostlyInFrustum );
  }

	Vec3 vCamPos = rCam.GetPosition();

	float fNodeDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos, m_objectsBox))*m_fZoomFactor;

	if(fNodeDistance < GetCVars()->e_scene_merging_min_merge_distance)// || fNodeDistance > m_fObjectsMaxViewDist*0.8)
		bNodeCompletelyInFrustum = false;

	if(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS)
		if(!GetCVars()->e_obj || fNodeDistance > m_fObjectsMaxViewDist)
			return false;

	if(nRenderMask & OCTREENODE_RENDER_FLAG_OCCLUDERS)
		if(fNodeDistance > m_fObjectsMaxViewDist*GetCVars()->e_cbuffer_occluders_view_dist_ratio)
			return false;

	if(m_nLastVisFrameId != GetFrameID() && &rCB == Get3DEngine()->GetCoverageBuffer())
	{
		if(GetObjManager()->IsBoxOccluded( m_objectsBox, fNodeDistance, &m_occlTestState, m_pVisArea != NULL, 
			(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS) ? eoot_OCCELL : eoot_OCCELL_OCCLUDER ))
		{
			m_nOccludedFrameId = GetFrameID();
			return false;
		}
	}

	m_nLastVisFrameId = GetFrameID();

	if(m_vSunDirReady.Dot(Get3DEngine()->GetSunDirNormalized())<0.99f)
		m_bCompiled = false;

	if(!m_bCompiled || 
    (!m_pCompiledInstances && GetCVars()->e_vegetation_static_instancing /*&& m_nodeBox.GetSize().x == fNodeMinSize*/))
		CompileObjects();

	CheckUpdateAreaBrushes();

	if(GetCVars()->e_obj_tree_min_node_size>0 && m_lstObjects.m_pFirstNode)
	{
		DrawBBox(m_nodeBox, Col_Blue);
		DrawBBox(m_objectsBox, Col_Red);
	}

  if(GetCVars()->e_obj_tree_max_node_size && GetCVars()->e_obj_tree_max_node_size > (0.501f*(m_nodeBox.max.z-m_nodeBox.min.z)))
  {
    DrawBBox(m_nodeBox, Col_Blue);
    DrawBBox(m_objectsBox, Col_Red);
  }

	bool bObjectsFound = false;

	if(nRenderMask & OCTREENODE_RENDER_FLAG_OCCLUDERS && m_lstOccluders.Count() &&
		fNodeDistance < m_fObjectsMaxViewDist*GetCVars()->e_cbuffer_occluders_view_dist_ratio)
	{
		for(int i=0; i<m_lstOccluders.Count(); i++)
		{
			IRenderNode * pObj = m_lstOccluders.GetAt(i);
			AABB objBox = pObj->GetBBox();
			float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,objBox))*m_fZoomFactor;
			assert(fEntDistance>=0 && _finite(fEntDistance));

			if(fEntDistance*GetObjManager()->m_fOcclTimeRatio < pObj->m_fWSMaxViewDist*GetCVars()->e_cbuffer_occluders_view_dist_ratio)
			{
				bool bCompletelyInFrustum = false;
				if(rCam.IsAABBVisible_EHM( objBox, &bCompletelyInFrustum ))
				{
          if(&rCB != Get3DEngine()->GetCoverageBuffer())
            bCompletelyInFrustum = false;

					GetObjManager()->RenderOccluderIntoZBuffer( pObj, fEntDistance, rCB, bCompletelyInFrustum );
					bObjectsFound = true;
				}
			}
		}
	}

	if(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS && m_lstObjects.m_pFirstNode && fNodeDistance < m_fObjectsMaxViewDist)
	{
    { // prepare m_dwSpritesAngle
      Vec3 vCenter = m_objectsBox.GetCenter();
      Vec3 vD = vCenter - vCamPos;
      m_dwSpritesAngle = ((uint32)(cry_atan2f(vD.x, vD.y)*(255.0f/(2*PI)))) & 0xff;
    }

    PodArray<CDLight*> * pAffectingLights = GetAffectingLights();

    bool bSunOnly = pAffectingLights && (pAffectingLights->Count()==1) && 
      (pAffectingLights->GetAt(0)->m_Flags & DLF_SUN) && !m_pVisArea && m_pTerrainNode;

    bool bStaticInstancing = false;

    if(/*!m_nRenderStackLevel && */(bNodeMostlyInFrustum || GetCVars()->e_vegetation_static_instancing>1) && GetCVars()->e_vegetation_static_instancing && m_pCompiledInstances && m_pCompiledInstances->size())
    {
      for(int nObjectTypeID=0; nObjectTypeID<m_pCompiledInstances->size(); nObjectTypeID++)
      {
        if((*m_pCompiledInstances)[nObjectTypeID].pStatInstGroup)
        {
          SInstancingInfo * pIi = &(*m_pCompiledInstances)[nObjectTypeID];
          CVegetation * pObj = pIi->arrInstances[0];

          const AABB & objBox = pIi->aabb;
          float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,objBox))*m_fZoomFactor;
          assert(fEntDistance>=0 && _finite(fEntDistance));

          if(fEntDistance < pObj->m_fWSMaxViewDist && rCam.IsAABBVisible_EM( objBox ) && pIi->arrInstances.size()>8)
          {
            if(fEntDistance < pIi->fMinSpriteDistance)
              GetObjManager()->RenderObject( pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, rCam, pIi, bSunOnly );
            else if(pIi->arrSprites.size())
            {
              GetObjManager()->m_lstFarObjects[m_nRenderStackLevel].AddList(&pIi->arrSprites[0], pIi->arrSprites.size());
              if (GetCVars()->e_bboxes)
                DrawBBox(objBox, Col_Yellow);
            }

            pIi->bInstancingInUse = true;
            bStaticInstancing = true;
          }
          else
            pIi->bInstancingInUse = false;
        }
      }
    }

   //static uint nGood=1;
    //static uint nAll=1;

    for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			// Prefetch next object into cache, helps in ascension, works faster than just thatching memory 
			if(pObj->m_pNext)
        cryPrefetchT0SSE(pObj->m_pNext);
/*
      uint64 nStep = ((uint64)pObj->m_pNext - (uint64)pObj);
      nGood += (nStep>0 && nStep<128);
      nAll++;
*/
      if(pObj->m_fWSMaxViewDist < fNodeDistance)
        continue;

      if(bStaticInstancing && pObj->GetRenderNodeType() == eERType_Vegetation)
      {
        assert(((CVegetation*)pObj)->m_nObjectTypeID<m_pCompiledInstances->size());
        SInstancingInfo * pIi = &(*m_pCompiledInstances)[((CVegetation*)pObj)->m_nObjectTypeID];
        if(pIi->bInstancingInUse)
          continue;
      }

			const AABB & objBox = pObj->GetBBox();
      if(bNodeCompletelyInFrustum || rCam.IsAABBVisible_FM( objBox ))
      {
        float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,objBox))*m_fZoomFactor;
        assert(fEntDistance>=0 && _finite(fEntDistance));
        if(fEntDistance < pObj->m_fWSMaxViewDist)
			  {
				  if(GetCVars()->e_scene_merging && !bNodeCompletelyInFrustum && pObj->m_dwRndFlags&ERF_MERGE_RESULT)
					  continue;

          if(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES)
            if(pObj->GetRenderNodeType() != eERType_Unknown)
               continue;

				  GetObjManager()->RenderObject( pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, rCam, NULL, bSunOnly );

				  bObjectsFound = true;
			  }
      }
		}
/*
    static int nPrintFrameId=0;
    if(nPrintFrameId != GetFrameID() && (GetFrameID()&15) == 15)
    {
      nPrintFrameId = GetFrameID();

      float fRatio = (float)nGood / (float)nAll;
      PrintMessage("nMemStep = %.2f", fRatio);

      nGood = 0;
      nAll = 0;
    }
*/
		if(GetCVars()->e_scene_merging && !bNodeCompletelyInFrustum)
		{
			for(int i=0; i<m_lstMergedObjects.Count(); i++)
			{
				IRenderNode * pObj = m_lstMergedObjects.GetAt(i);
				AABB objBox = pObj->GetBBox();
				float fEntDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos,objBox))*m_fZoomFactor;
				assert(fEntDistance>=0 && _finite(fEntDistance));

				if(fEntDistance < pObj->m_fWSMaxViewDist)
					if(rCam.IsAABBVisible_EM( objBox ))
					{
						if(!bNodeCompletelyInFrustum && pObj->m_dwRndFlags&ERF_MERGE_RESULT)
							continue;

						GetObjManager()->RenderObject( pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, rCam, NULL, bSunOnly );

						bObjectsFound = true;
					}
			}
		}
	}

	if(GetCVars()->e_obj_tree_min_node_size && 0.501f*(m_nodeBox.max.z-m_nodeBox.min.z)<GetCVars()->e_obj_tree_min_node_size)
		return bObjectsFound;

	int nFirst = 
		((vCamPos.x > m_vNodeCenter.x) ? 4 : 0) |
		((vCamPos.y > m_vNodeCenter.y) ? 2 : 0) |
		((vCamPos.z > m_vNodeCenter.z) ? 1 : 0);

	if(m_arrChilds[nFirst  ])
		bObjectsFound |= m_arrChilds[nFirst  ]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^1])
		bObjectsFound |= m_arrChilds[nFirst^1]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^2])
		bObjectsFound |= m_arrChilds[nFirst^2]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^4])
		bObjectsFound |= m_arrChilds[nFirst^4]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^3])
		bObjectsFound |= m_arrChilds[nFirst^3]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^5])
		bObjectsFound |= m_arrChilds[nFirst^5]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^6])
		bObjectsFound |= m_arrChilds[nFirst^6]->Render(rCam, rCB, nRenderMask, vAmbColor);

	if(m_arrChilds[nFirst^7])
		bObjectsFound |= m_arrChilds[nFirst^7]->Render(rCam, rCB, nRenderMask, vAmbColor);

	return bObjectsFound;
}

void COctreeNode::CompileCharacter( ICharacterInstance * pChar, unsigned char &nInternalFlags )
{
  if( pChar )
  {
    IMaterial *pCharMaterial = pChar->GetMaterial();
    if(pCharMaterial && ((CMatInfo*)pCharMaterial)->IsPerObjectShadowPassNeeded())
    {
      nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
      return;
    }

    if(IAttachmentManager * pAttMan = pChar->GetIAttachmentManager())
    {
      int nCount = pAttMan->GetAttachmentCount();
      for(int i=0; i<nCount; i++)
      {
        if(IAttachment * pAtt = pAttMan->GetInterfaceByIndex(i))
          if(IAttachmentObject * pAttObj = pAtt->GetIAttachmentObject())
          {
            ICharacterInstance *pCharInstance = pAttObj->GetICharacterInstance();
            if( pCharInstance )
            {
//              CompileCharacter( pCharInstance, nInternalFlags);
              IMaterial *pCharMaterial = pCharInstance->GetMaterial();
              if(pCharMaterial && ((CMatInfo*)pCharMaterial)->IsPerObjectShadowPassNeeded())
              {
                nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
                return;
              }

            }

            if(IStatObj * pStatObj = pAttObj->GetIStatObj())
            {
              IMaterial * pMat = pAttObj->GetMaterial();
              if(!pMat)
                pMat = pStatObj->GetMaterial();

              if(pMat && ((CMatInfo*)pMat)->IsPerObjectShadowPassNeeded())
              {
                nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
                return;
              }
            }
          }
      }
    }
  }
}

void COctreeNode::CompileObjects()
{
	FUNCTION_PROFILER_3DENGINE;

	m_lstOccluders.Clear();

	m_bHasIndoorObjects = m_bHasOutdoorObjects = false;

  SAFE_DELETE(m_pCompiledInstances);
  if(GetCVars()->e_vegetation_static_instancing /*&& m_nodeBox.GetSize().x == fNodeMinSize*/)
  {
    m_pCompiledInstances = new DynArray<SInstancingInfo>;
    m_aabbCompiledInstances.Reset();
  }

  SSectorTextureSet * pTerrainTexInfo = NULL;
  if(CTerrainNode * pTerNode = GetTerrainNode())
  {
    while( !pTerNode->m_nNodeTexSet.nTex0 && pTerNode->m_pParent )
      pTerNode = pTerNode->m_pParent;

    pTerrainTexInfo = &(pTerNode->m_nNodeTexSet);
  }

  float fObjMaxViewDistance = 0;

	// update node
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode, * pNext; pObj; pObj = pNext)
  {
    pNext = pObj->m_pNext;

    bool bVegetHasAlphaTrans = false;

		// update vegetation instances data
		EERType eRType = pObj->GetRenderNodeType();
		if(eRType == eERType_Vegetation)
		{
			CVegetation * pInst = (CVegetation*)pObj;				
			pInst->UpdateRndFlags();
			if(GetObjManager()->m_lstStaticTypes[pInst->m_nObjectTypeID].pStatObj)
				if(GetObjManager()->m_lstStaticTypes[pInst->m_nObjectTypeID].bUseAlphaBlending)
					bVegetHasAlphaTrans = true;

      if(GetCVars()->e_vegetation_use_terrain_color)
  			pInst->UpdateInstanceBrightness();							

      if(m_pCompiledInstances)
      {
        SInstanceInfo ii;
        pInst->CalcMatrix(ii.m_Matrix);
        ii.m_AmbColor = Col_White;

        if(pInst->m_nObjectTypeID >= m_pCompiledInstances->size())
          m_pCompiledInstances->resize(pInst->m_nObjectTypeID+1);        
        SInstancingInfo & rInfo = (*m_pCompiledInstances)[pInst->m_nObjectTypeID];
        rInfo.arrInstances.push_back(pInst);
        rInfo.arrMats.push_back(ii);
        rInfo.pStatInstGroup = &GetObjManager()->m_lstStaticTypes[pInst->m_nObjectTypeID];
        rInfo.aabb.Add(pInst->GetBBox());

        StatInstGroup & vegetGroup = GetObjManager()->m_lstStaticTypes[pInst->m_nObjectTypeID];
        if(vegetGroup.bUseSprites)
          rInfo.fMinSpriteDistance = vegetGroup.m_fSpriteSwitchDistUnScaled;
        else
          rInfo.fMinSpriteDistance = 1000000.f;

        if(vegetGroup.bUseSprites)
        { // just register to be rendered as sprite and return
          SVegetationSpriteInfo si;
          si.ucSpriteAlphaTestRef = 25;
          si.pTerrainTexInfoForSprite = (vegetGroup.bUseTerrainColor && GetCVars()->e_vegetation_use_terrain_color) ? pTerrainTexInfo : NULL;
          si.pVegetation = pInst;
          si.dwAngle = m_dwSpritesAngle;
          rInfo.arrSprites.push_back(si);
        }

        m_aabbCompiledInstances.Add(pInst->GetBBox());
      }
		}

		// update max view distances
		pObj->m_fWSMaxViewDist = pObj->GetMaxViewDist();

		// update PER_OBJECT_SHADOW_PASS_NEEDED flag
    if(GetCVars()->e_shadows_on_alpha_blended)
    {
		  pObj->m_nInternalFlags &= ~IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
		  if(	eRType != eERType_Light &&
			    eRType != eERType_Cloud &&
			    eRType != eERType_VoxelObject &&
			    eRType != eERType_FogVolume &&
			    eRType != eERType_Decal &&
			    eRType != eERType_RoadObject_NEW &&
			    eRType != eERType_DistanceCloud &&
			    eRType != eERType_AutoCubeMap )
		  {
        if (eRType==eERType_ParticleEmitter)
          pObj->m_nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;

        if(CMatInfo * pMatInfo = (CMatInfo*)pObj->GetMaterial())
          if(bVegetHasAlphaTrans || pMatInfo->IsPerObjectShadowPassNeeded())
            pObj->m_nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
				
				if(eRType==eERType_Unknown)
				{
					int nSlotCount = pObj->GetSlotCount();

					for(int s=0; s<nSlotCount; s++)
					{
						if(CMatInfo * pMat = (CMatInfo*)pObj->GetEntitySlotMaterial(s))
							if(pMat->IsPerObjectShadowPassNeeded())
								pObj->m_nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;

						if(IStatObj * pStatObj = pObj->GetEntityStatObj(s))
							if(CMatInfo * pMat = (CMatInfo*)pStatObj->GetMaterial())
								if(pMat->IsPerObjectShadowPassNeeded())
									pObj->m_nInternalFlags |= IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED;
					}
				}

        if(!(pObj->m_nInternalFlags & IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED))
          CompileCharacter( pObj->GetEntityCharacter(0), pObj->m_nInternalFlags );
		  }
    }

		int nFlags = pObj->GetRndFlags();

		// fill shadow casters list
		if(nFlags&ERF_CASTSHADOWMAPS && pObj->m_fWSMaxViewDist>fMinShadowCasterViewDist && eRType != eERType_Light)
		{
			m_bHasShadowCasters = true;
		}

		// fill occluders list
		if ((pObj->m_nInternalFlags&IRenderNode::HAS_OCCLUSION_PROXY) || (eRType == eERType_VoxelObject && (nFlags&ERF_GOOD_OCCLUDER)))
		{
			m_lstOccluders.Add(pObj);
			m_bHasOccluders = true;
		}

		if(nFlags&ERF_OUTDOORONLY)
			m_bHasOutdoorObjects = true;
		else
			m_bHasIndoorObjects = true;

    fObjMaxViewDistance = max(fObjMaxViewDistance,pObj->m_fWSMaxViewDist);
	}

  if(fObjMaxViewDistance>m_fObjectsMaxViewDist)
  {
    COctreeNode * pNode = this;
    while(pNode)
    {
      pNode->m_fObjectsMaxViewDist = max(pNode->m_fObjectsMaxViewDist, fObjMaxViewDistance);
      pNode = pNode->m_pParent;
    }
  }

	m_bCompiled = true;

	m_vSunDirReady = Get3DEngine()->GetSunDirNormalized();
}
/*
uint COctreeNode::GetLightMask()
{ 
	uint nSunLightMask = Get3DEngine()->GetTerrain()->GetSunLightMask();
	return (m_nLightMaskFrameId == GetFrameID()) ? (m_nLightMask | nSunLightMask) : nSunLightMask; 
}

void COctreeNode::AddLightMask(CDLight * pLight) 
{ 
	if(!m_objectsBox.IsOverlapSphereBounds(pLight->m_Origin,pLight->m_fRadius))
		return;

	uint nNewMask = 1<<pLight->m_Id;

	if(m_nLightMaskFrameId == GetFrameID())
	{
		m_nLightMask |= nNewMask; 
	}
	else
	{
		m_nLightMask = nNewMask; 
		m_nLightMaskFrameId = GetFrameID();
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->AddLightMask( pLight );
}
*/


void COctreeNode::CheckInitAffectingLights()
{
  if(m_nLightMaskFrameId != GetFrameID())
  {
    m_lstAffectingLights.Clear();

    if( !m_pVisArea || m_pVisArea->IsAffectedByOutLights() )
    {
      PodArray<CDLight> * pSceneLights = Get3DEngine()->GetDynamicLightSources();
      if(pSceneLights->Count() && (pSceneLights->Get(0)->m_Flags & DLF_SUN))
        m_lstAffectingLights.Add(pSceneLights->Get(0));
    }

    m_nLightMaskFrameId = GetFrameID();
  }
}

PodArray<CDLight*> * COctreeNode::GetAffectingLights()
{ 
  CheckInitAffectingLights();

  return &m_lstAffectingLights; 
}

void COctreeNode::AddLightSource(CDLight * pSource)
{ 
  if(!m_objectsBox.IsOverlapSphereBounds(pSource->m_Origin,pSource->m_fRadius))
    return;

  CheckInitAffectingLights();

  if(m_lstAffectingLights.Find(pSource)<0)
  {
    m_lstAffectingLights.Add(pSource); 

    for(int i=0; i<8; i++) 
      if(m_arrChilds[i])
        m_arrChilds[i]->AddLightSource( pSource );
  }
}

bool IsAABBInsideHull(SPlaneObject * pHullPlanes, int nPlanesNum, const AABB & aabbBox);

void COctreeNode::FillShadowCastersList(CDLight * pLight, ShadowMapFrustum * pFr, PodArray<SPlaneObject> * pShadowHull, bool bUseFrustumTest)
{
	FUNCTION_PROFILER_3DENGINE;

  if(bUseFrustumTest && !pFr->Intersect(m_objectsBox))
	  return;

	if(pShadowHull && !pFr->bUseAdditiveBlending)
		if(!IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), m_objectsBox))
			return;

	if(!m_bCompiled)
		CompileObjects();

	CheckUpdateAreaBrushes();

  bool bSun = (pLight->m_Flags&DLF_SUN) != 0;

	Vec3 vCamPos = GetCamera().GetPosition();
	if(GetCVars()->e_shadows_cast_view_dist_ratio)// && bSun)
	{
		float fNodeDistance = cry_sqrtf(Distance::Point_AABBSq(vCamPos, m_objectsBox))*m_fZoomFactor;
    float fGSMBoxSize = 1.0f / (pFr->fFrustrumSize * GetCVars()->e_gsm_range);
    fNodeDistance = max(fNodeDistance, fGSMBoxSize);
		if(fNodeDistance > m_fObjectsMaxViewDist*GetCVars()->e_shadows_cast_view_dist_ratio)
			return;
	}

	assert(pFr->pLightOwner == pLight->m_pOwner);

  CLightEntity * pLightEnt = (CLightEntity *)pLight->m_pOwner;

	// Initialize occlusion culler pointer if light source==sun and occ_check==1
	CCullBuffer* pCB	=	bSun && (GetCVars()->e_shadows_occ_check==1) ? Get3DEngine()->GetCoverageBuffer() : 0;

	if((bSun && (m_bHasOutdoorObjects || (m_pVisArea && m_pVisArea->IsAffectedByOutLights()))) || (!bSun && m_bHasIndoorObjects) || !(GetCVars()->e_cbuffer==2))
	{
    for(IRenderNode * pCaster = m_lstObjects.m_pFirstNode; pCaster; pCaster = pCaster->m_pNext)
		{
#ifdef _DEBUG
			const char * szClass = pCaster->GetEntityClassName();
			const char * szName  = pCaster->GetName();
#endif // _DEBUG

      // Prefetch next object into cache
      //if (pCaster->m_pNext)
      //  cryPrefetchT0SSE(pCaster->m_pNext);

      EERType eRNType = pCaster->GetRenderNodeType();
      bool bCaster(eRNType == eERType_Vegetation || eRNType == eERType_Brush  || eRNType == eERType_VoxelObject || 
                   eRNType == eERType_Unknown || eRNType == eERType_ParticleEmitter || eRNType == eERType_Rope );
      if(!bCaster)
        continue;

			int dwRndFlags = pCaster->GetRndFlags();

      // objects are sorted by casters first so stop search once first non caster found
      if(!(dwRndFlags&ERF_CASTSHADOWMAPS) && GetCVars()->e_shadows<2)
      {
        if(eRNType == eERType_Vegetation || eRNType == eERType_Brush)
          break;
        else
          continue;
      }

			if (pFr->bForSubSurfScattering && !(dwRndFlags & (ERF_CASTSCATTERMAPS | ERF_SUBSURFSCATTER)))
				continue;

      const AABB & objBox = pCaster->GetBBox();

			if(GetCVars()->e_shadows_cast_view_dist_ratio)// && bSun) // do it only for sun
			{
				float fDistanceSq = Distance::Point_AABBSq(vCamPos,objBox);
				if(fDistanceSq > pow(pCaster->m_fWSMaxViewDist*GetCVars()->e_shadows_cast_view_dist_ratio/m_fZoomFactor,2))
					continue;
			}

			//sub surf scattering objects can be in few GSM frustums independently
			if (!pFr->bForSubSurfScattering && bSun && pCaster->m_pRNTmpData)
        if(pCaster->m_pRNTmpData->userData.m_nGSMFrameId == GetFrameID())
				  continue; // already present completely in some LOD

		  if(bUseFrustumTest && !pFr->Intersect(objBox))
			  continue; // not inside shadow map projection

			if(pShadowHull && !pFr->bUseAdditiveBlending && !pFr->bForSubSurfScattering)
				if(	!IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), objBox) )
					continue; // not inside shadow casting hull

			if(pCB && !pCB->IsShadowcasterVisible(objBox,Get3DEngine()->GetSunDirNormalized()*GetCVars()->e_shadows_cast_view_dist_ratio))
				continue;
	
			//casting shadows should be independent from scattering depth map
			if(!pFr->bForSubSurfScattering && bSun && pFr->FrustumPlanes.IsAABBVisible_EH(objBox) == CULL_INCLUSION)
      {
        Get3DEngine()->CheckCreateRNTmpData(&pCaster->m_pRNTmpData, pCaster);
				pCaster->m_pRNTmpData->userData.m_nGSMFrameId = GetFrameID(); // mark as completely present in some LOD 
      }

			if(dwRndFlags & (ERF_HIDDEN)) 
				continue;

			if(dwRndFlags & ERF_CASTSHADOWMAPS)
				if(GetCVars()->e_vegetation || pCaster->GetRenderNodeType() != eERType_Vegetation)
					if(GetCVars()->e_brushes || pCaster->GetRenderNodeType() != eERType_Brush)
            if(GetCVars()->e_entities || pCaster->GetRenderNodeType() != eERType_Unknown)
					{
						//				if(pCaster->m_pAffectingLights)
						//			if(!(pLight->m_Flags&DLF_SUN) || !(dwRndFlags&ERF_OUTDOORONLY))
						//		if(pCaster->m_pAffectingLights->Find((ILightSource*)pFr->pLightOwner)<0)
						//		continue;

            if(pCaster == pLightEnt->m_pNotCaster)
              continue;

						pFr->pCastersList->Add(pCaster);
					}
		}
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i] && m_arrChilds[i]->m_bHasShadowCasters)
	    m_arrChilds[i]->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest);
}

void COctreeNode::MarkAsUncompiled()
{
	m_bCompiled = false;

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->MarkAsUncompiled();
}

COctreeNode * COctreeNode::FindNodeContainingBox(const AABB & objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	if(!m_nodeBox.IsContainSphere(objBox.min, -0.01f) || !m_nodeBox.IsContainSphere(objBox.max, -0.01f))
		return NULL;

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			if(COctreeNode * pFoundNode = m_arrChilds[i]->FindNodeContainingBox(objBox))
				return pFoundNode;

	return this;
}

void COctreeNode::MoveObjectsIntoList(PodArray<SRNInfo> * plstResultEntities, const AABB * pAreaBox, 
                                      bool bRemoveObjects, bool bSkipDecals, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects,
                                      EERType eRNType)
{
	FUNCTION_PROFILER_3DENGINE;

	if(pAreaBox && !Overlap::AABB_AABB(m_objectsBox, *pAreaBox))
		return;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode, * pNext; pObj; pObj = pNext)
  {
    pNext = pObj->m_pNext;

    if(eRNType < eERType_Last && pObj->GetRenderNodeType() != eRNType)
      continue;

		if(bSkipDecals && pObj->GetRenderNodeType() == eERType_Decal)
			continue;

		if(bSkip_ERF_NO_DECALNODE_DECALS && pObj->GetRndFlags()&ERF_NO_DECALNODE_DECALS)
			continue;

		if(bSkipDynamicObjects)
		{
      EERType eRType = pObj->GetRenderNodeType();

      if(eRType == eERType_Unknown)
      {
        if(pObj->IsMovableByGame())
          continue;
      }
      else if(	
        eRType != eERType_Brush &&
				eRType != eERType_Vegetation &&
				eRType != eERType_VoxelObject )
				continue;
		}

    if(pAreaBox && !Overlap::AABB_AABB(pObj->GetBBox(), *pAreaBox))
      continue;

    if(bRemoveObjects)
    {
      UnlinkObject(pObj);
      m_bCompiled = false;
    }

    plstResultEntities->Add(pObj);
	}

	for(int i=0; i<8; i++) 
  {
		if(m_arrChilds[i])
    {
      m_arrChilds[i]->MoveObjectsIntoList(plstResultEntities, pAreaBox, bRemoveObjects, bSkipDecals, bSkip_ERF_NO_DECALNODE_DECALS, bSkipDynamicObjects, eRNType);

//      if(bRemoveObjects && !m_arrChilds[i]->m_lstObjects.m_pFirstNode && !m_lstMergedObjects.Count())
  //      SAFE_DELETE(m_arrChilds[i]);
    }
  }
}

void COctreeNode::DeleteObjectsByFlag(int nRndFlag)
{
	FUNCTION_PROFILER_3DENGINE;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode, * pNext; pObj; pObj = pNext)
  {
    pNext = pObj->m_pNext;

		if(pObj->GetRndFlags()&nRndFlag)
			DeleteObject(pObj);
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->DeleteObjectsByFlag(nRndFlag);
}

int COctreeNode::PhysicalizeVegetationInBox(const AABB &bbox)
{
	if(!Overlap::AABB_AABB(m_objectsBox, bbox))
		return 0;	

	int nCount=0;
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	if(pObj->GetRenderNodeType()==eERType_Vegetation && !(pObj->GetRndFlags()&ERF_PROCEDURAL))
  {
    const AABB & objBox = pObj->GetBBox();
    if(Overlap::AABB_AABB(bbox, objBox) && 
      max(objBox.max.x-objBox.min.x, objBox.max.y-objBox.min.y) <= 
      ((C3DEngine*)gEnv->p3DEngine)->GetCVars()->e_on_demand_maxsize)
	  {
		  if (!pObj->GetPhysics())
			  pObj->Physicalize(true);
		  if (pObj->GetPhysics())
			  pObj->GetPhysics()->AddRef(), nCount++;
	  }
  }

	for(int i=0; i<8; i++) if(m_arrChilds[i])
		nCount += m_arrChilds[i]->PhysicalizeVegetationInBox(bbox);
	return nCount;
}

int COctreeNode::DephysicalizeVegetationInBox(const AABB &bbox)
{
	if(!Overlap::AABB_AABB(m_objectsBox, bbox))
		return 0;	

  int nCount=0;
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
  if(pObj->GetRenderNodeType()==eERType_Vegetation && !(pObj->GetRndFlags()&ERF_PROCEDURAL))
  {
    const AABB & objBox = pObj->GetBBox();
		if(Overlap::AABB_AABB(bbox, objBox) && 
		  max(objBox.max.x-objBox.min.x, objBox.max.y-objBox.min.y) <=
		  ((C3DEngine*)gEnv->p3DEngine)->GetCVars()->e_on_demand_maxsize)
		pObj->Dephysicalize(true);
  }

	for(int i=0; i<8; i++) if(m_arrChilds[i])
		nCount += m_arrChilds[i]->DephysicalizeVegetationInBox(bbox);
	return nCount;
}

AABB COctreeNode::GetChildBBox(int nChildId)
{
	int x = (nChildId/4);
	int y = (nChildId-x*4)/2;
	int z = (nChildId-x*4-y*2);
	Vec3 vSize = m_nodeBox.GetSize()*0.5f;
	Vec3 vOffset = vSize;
	vOffset.x *= x;
	vOffset.y *= y;
	vOffset.z *= z;
	AABB childBox;
	childBox.min = m_nodeBox.min + vOffset;
	childBox.max = childBox.min + vSize;
	return childBox;
}

int COctreeNode::GetObjectsCount(EOcTeeNodeListType eListType)
{
  int nCount = 0;

  switch(eListType)
  {
  case eMain:
    for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
      nCount++;
    break;
  case eCasters:
    for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
    {
      if(!(pObj->GetRndFlags()&ERF_CASTSHADOWMAPS))
        break;
      nCount++;
    }
    break;
  case eOccluders:
    nCount = m_lstOccluders.Count();
    break;
  case eMergedCache:
    nCount = m_lstMergedObjects.Count();
    break;
  }

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			nCount += m_arrChilds[i]->GetObjectsCount(eListType);

	return nCount;
}

bool COctreeNode::IsRightNode(const AABB & objBox, const float fObjRadius, float fObjMaxViewDist)
{
  if(!Overlap::Point_AABB(objBox.GetCenter(), m_nodeBox))
    if(this != Get3DEngine()->m_pObjectsTree)
      return false; // fail if center is not inside or node bbox

  if(2 != Overlap::AABB_AABB_Inside(objBox, m_objectsBox))
    return false; // fail if not completely inside of objects bbox

  float fNodeRadiusRated = m_fNodeRadius*fObjectToNodeSizeRatio;

  if(fObjRadius > fNodeRadiusRated*2.f)
    if(this != Get3DEngine()->m_pObjectsTree)
      return false; // fail if object is too big and we need to register it some of parents

	if(m_nodeBox.max.x-m_nodeBox.min.x > fNodeMinSize)
		if(fObjRadius < fNodeRadiusRated)
//      if(fObjMaxViewDist < m_fNodeRadius*GetCVars()->e_view_dist_ratio_vegetation*fObjectToNodeSizeRatio)
        return false; // fail if object is too small and we need to register it some of childs

	return true;
}

int COctreeNode::GetMemoryUsage(ICrySizer * pSizer)
{
	int nSize = sizeof(*this);

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
		if(!(pObj->GetRndFlags()&ERF_PROCEDURAL))
			pObj->GetMemoryUsage(pSizer);

	nSize += sizeofVector(m_lstOccluders);
	nSize += sizeofVector(m_lstMergedObjects);

	if(m_plstAreaBrush)
		nSize += sizeofVector(*m_plstAreaBrush);

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			nSize += m_arrChilds[i]->GetMemoryUsage(pSizer);

	if (pSizer)
		pSizer->AddObject(this,nSize);

	return nSize;
}

IRenderNode * COctreeNode::FindTerrainSectorVoxObject(const AABB & objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	if(!Overlap::AABB_AABB(m_objectsBox, objBox))
		return NULL;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if(pObj->GetRenderNodeType() == eERType_VoxelObject && 
			((CVoxelObject*)pObj)->IsSnappedToTerrainSectors() &&
			((CVoxelObject*)pObj)->GetRenderMesh(0) &&
			((CVoxelObject*)pObj)->GetRenderMesh(0)->GetSysIndicesCount())
		{
			if(fabs(objBox.min.x - pObj->GetBBox().min.x)<0.01f)
				if(fabs(objBox.min.y - pObj->GetBBox().min.y)<0.01f)
					if(fabs(objBox.max.x - pObj->GetBBox().max.x)<0.01f)
						if(fabs(objBox.max.y - pObj->GetBBox().max.y)<0.01f)
							return pObj;
		}
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			if(IRenderNode * pObj = m_arrChilds[i]->FindTerrainSectorVoxObject(objBox))
				return pObj;

	return NULL;
}


int Cmp_RenderChunksInfo_LMInfo(const void* v1, const void* v2)
{
	SRenderMeshInfoInput *pChunk1 = (SRenderMeshInfoInput*)v1;
	SRenderMeshInfoInput *pChunk2 = (SRenderMeshInfoInput*)v2;

	return memcmp(&pChunk1->LMInfo,&pChunk2->LMInfo,sizeof(pChunk2->LMInfo));
}

void COctreeNode::CheckUpdateAreaBrushes()
{
	if(!GetCVars()->e_scene_merging)
		return FreeAreaBrushes();

	Vec3 vCamPos = GetCamera().GetPosition();

	float fDist = cry_sqrtf(Distance::Point_AABBSq(vCamPos, m_objectsBox))*m_fZoomFactor /*- m_fNodeRadius*/;

	if(m_bAreaMeshReady)
		if(m_nRenderStackLevel || fabs(m_fReadyMergingDistance-fDist)<m_fNodeRadius)
			return;

	if(m_fAreaMergingTimeLimit<=0)
	{
		if(GetCVars()->e_scene_merging==2)
			DrawBBox(m_nodeBox);
		return;
	}

	FUNCTION_PROFILER_3DENGINE;

	FreeAreaBrushes();

	float fStartTime = GetCurAsyncTimeSec();

	m_bAreaMeshReady = true;

	m_plstAreaBrush = new PodArray<CBrush*>;

	float fAreaMaxViewDist = 0;

	PodArray<SRenderMeshInfoOutput> lstResultRenderMeches;
	CRenderMeshMerger Merger;

	m_fReadyMergingDistance = fDist;

	// build list of all objects in node
	PodArray<SRenderMeshInfoInput> lstRMI;
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode, * pNext; pObj; pObj = pNext)
  {
    pNext = pObj->m_pNext;

		const char * pName = pObj->GetName();
//		if(pObj->GetRenderNodeType() != eERType_Brush)
		if(pObj->GetRenderNodeType() != eERType_Vegetation)
			continue;

		assert(pObj->GetEntityVisArea() == (IVisArea*)this || pObj->m_pOcNode == (COctreeNode*)this);
		assert(!(pObj->GetRndFlags() & ERF_MERGE_RESULT));

		if(pObj->GetRndFlags() & ERF_MERGE_RESULT || pObj->GetRndFlags() & ERF_HIDDEN)
			continue;

		float fInstDist = m_fReadyMergingDistance;//cry_sqrtf(Distance::Point_AABBSq(vCamPos,pObj->GetBBox()))*m_fZoomFactor;

		if(fInstDist > pObj->m_fWSMaxViewDist)
			continue;

		Matrix34 objMat; objMat.SetIdentity();
		IStatObj * pEntObject = pObj->GetEntityStatObj(0, 0, &objMat);
		if(!pEntObject)
			continue;

		int nLod = FtoI(pObj->GetLodForDistance(max(0.f,fInstDist)));

		if(nLod<0)
			continue; // sprite

		pEntObject = pEntObject->GetLodObject(nLod);
		if(!pEntObject)
			continue;

		float fMatScale = objMat.GetColumn0().GetLength();
		if(fMatScale<0.01f || fMatScale>100.f)
			continue;

		SRenderMeshInfoInput rmi;
		//		rmi.pMat = pEntObject->GetMaterial();
		rmi.pMat = pObj->GetMaterial();
		rmi.mat = objMat;
		rmi.pMesh = pEntObject->GetRenderMesh();
		rmi.pSrcRndNode = pObj;

		if(pObj->GetRndFlags() & ERF_USERAMMAPS)
			if(SLMData * pLMData = pObj->GetLightmapData(0))
				if(pLMData->m_pLMTCBuffer && pLMData->m_pLMData)
		{
			rmi.pLMTexCoordsRM = pLMData->m_pLMTCBuffer;
			rmi.LMInfo.iRAETex = pLMData->m_pLMData->GetRAETex();
		}

    rmi.LMInfo.nRndFlags |= pObj->GetRndFlags()&(ERF_CASTSHADOWMAPS);
    rmi.LMInfo.nRndFlags |= pObj->GetRndFlags()&(ERF_USE_TERRAIN_COLOR);

		// check if it makes sense to merge this object
		if(rmi.pMesh)
		{
			float fDrawTrisPerChunk = rmi.pMesh->GetAverageTrisNumPerChunk(rmi.pMat);
			if(fDrawTrisPerChunk>GetCVars()->e_scene_merging_max_tris_in_chunk)
				continue;
		}
		else
		{
			float fDrawTrisPerChunk = 0;
			float fDrawChunksNum = 0;
			
			for(int s=0; s<pEntObject->GetSubObjectCount(); s++)
			{
				CStatObj::SSubObject * pSubObj = pEntObject->GetSubObject(s);
				if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					rmi.pMesh = pSubObj->pStatObj->GetRenderMesh();
					rmi.mat = objMat * pSubObj->tm;
					if(rmi.pMesh)
					{
						fDrawTrisPerChunk += rmi.pMesh->GetAverageTrisNumPerChunk(rmi.pMat);
						fDrawChunksNum++;
					}
				}
			}

			if(!fDrawChunksNum || fDrawTrisPerChunk/fDrawChunksNum > GetCVars()->e_scene_merging_max_tris_in_chunk)
				continue;
		}

		if(fAreaMaxViewDist < pObj->m_fWSMaxViewDist)
			fAreaMaxViewDist = pObj->m_fWSMaxViewDist;

    UnlinkObject(pObj);

    m_lstMergedObjects.Add(pObj);

		if(rmi.pMesh)
			lstRMI.Add(rmi);
		else
		{
			for(int s=0; s<pEntObject->GetSubObjectCount(); s++)
			{
				CStatObj::SSubObject * pSubObj = pEntObject->GetSubObject(s);
				if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					rmi.pMesh = pSubObj->pStatObj->GetRenderMesh();
					rmi.mat = objMat * pSubObj->tm;
					if(rmi.pMesh)
						lstRMI.Add(rmi);
				}
			}
		}
	}

	if(lstRMI.Count()>8)
	{
		qsort(lstRMI.GetElements(), lstRMI.Count(), sizeof(lstRMI[0]), Cmp_RenderChunksInfo_LMInfo);

		int nRMI_Id = 0;
		while(nRMI_Id < lstRMI.Count())
		{
			int nRndFlags = 0;
			SRenderMeshInfoInput::SSharedLMInfo CurrLMInfo = lstRMI[nRMI_Id].LMInfo;
			int nRMI_Id_Next = nRMI_Id;
			for(; nRMI_Id_Next < lstRMI.Count(); nRMI_Id_Next++)
			{
				if(memcmp(&lstRMI[nRMI_Id_Next].LMInfo,&CurrLMInfo,sizeof(CurrLMInfo)))
					break;

        nRndFlags |= lstRMI[nRMI_Id_Next].LMInfo.nRndFlags;
			}

			char szName[256]=""; sprintf(szName, "MergedBrush_%d", m_plstAreaBrush->Count());

			lstResultRenderMeches.Clear();

			CRenderMeshMerger::SMergeInfo info;
			info.sMeshName = szName;
			info.sMeshType = "Merged_Brushes_And_Vegetations";
//			info.bPlaceInstancePositionIntoVertexNormal = true;
			Merger.MergeRenderMeshes(&lstRMI[nRMI_Id], nRMI_Id_Next-nRMI_Id,lstResultRenderMeches, info);

			if(lstResultRenderMeches.Count())
			{
				float fDrawTrisPerChunk = 0;
				for(int m=0; m<lstResultRenderMeches.Count(); m++)
				{
					IRenderMesh * pNewRM = lstResultRenderMeches[m].pMesh;
					fDrawTrisPerChunk += pNewRM->GetAverageTrisNumPerChunk(pNewRM->GetMaterial());
				}
				fDrawTrisPerChunk /= lstResultRenderMeches.Count();
				if(fDrawTrisPerChunk<GetCVars()->e_scene_merging_max_tris_in_chunk)
					Warning("COctreeNode::MakeAreaBrush: Warning: Merging produces not optimal mesh: %.2f tris in average per chunk", fDrawTrisPerChunk);
			}

			for(int m=0; m<lstResultRenderMeches.Count(); m++)
			{
				IRenderMesh * pNewRM = lstResultRenderMeches[m].pMesh;

				if(!pNewRM || !pNewRM->GetVertCount())
				{ // make empty brush list - no geometry in sector
					continue;
				}

				// make new statobj
				CStatObj * pAreaStatObj = new CStatObj();
				pAreaStatObj->m_nLoadedTrisCount = pNewRM->GetSysIndicesCount()/3;
				pAreaStatObj->SetRenderMesh(pNewRM);
				Vec3 vMin, vMax; pNewRM->GetBBox(vMin, vMax);
				pAreaStatObj->SetBBoxMin(vMin);
				pAreaStatObj->SetBBoxMax(vMax);
				pAreaStatObj->AddRef();

				// make brush
				CBrush * pAreaBrush = new CBrush();

//				Matrix34 mat;
	//			mat.SetIdentity();
				//		mat.SetTranslation(Vec3(0,0,-0.5f));
				pAreaBrush->SetEntityStatObj(0,pAreaStatObj);
				pAreaBrush->SetBBox(AABB(vMin, vMax));
				pAreaBrush->SetMaterial(pNewRM->GetMaterial());

				if(float * pLMTexCoords = (float*)lstResultRenderMeches[m].pLMTexCoords)
				{
					assert(lstResultRenderMeches[m].pLMTexCoords);

					RenderLMData * pNewLMData = lstRMI[nRMI_Id].pSrcRndNode->GetLightmapData(0)->m_pLMData;
					assert(pNewLMData->GetRAETex() == CurrLMInfo.iRAETex);
					pAreaBrush->SetLightmap(pNewLMData, pLMTexCoords, pNewRM->GetVertCount(), 0,0);
					pAreaBrush->SetRndFlags(ERF_USERAMMAPS, true);
					assert(pAreaBrush->HasLightmap(0));
				}

				pAreaBrush->SetRndFlags(ERF_MERGE_RESULT|nRndFlags, true);

				Get3DEngine()->UnRegisterEntity(pAreaBrush);

				pAreaBrush->m_fWSMaxViewDist = pAreaBrush->GetMaxViewDist();

				{
					LinkObject(pAreaBrush);		
					pAreaBrush->m_fWSMaxViewDist = fAreaMaxViewDist;

					// fill shadow casters list
					if(nRndFlags&ERF_CASTSHADOWMAPS && pAreaBrush->m_fWSMaxViewDist>fMinShadowCasterViewDist)
					{
						m_bHasShadowCasters = true;
					}
				}

				pAreaBrush->m_pOcNode = (COctreeNode*)this;

				m_plstAreaBrush->Add(pAreaBrush);
			}

			nRMI_Id = nRMI_Id_Next;
		}
	}
	else
	{
    for(int i=0; i<m_lstMergedObjects.Count(); i++)
		  LinkObject(m_lstMergedObjects[i]);
		m_lstMergedObjects.Clear();
	}

	m_fAreaMergingTimeLimit -= (GetCurAsyncTimeSec()-fStartTime);
}

void COctreeNode::FreeAreaBrushes(bool bRecursive)
{
	if(bRecursive)
		for(int i=0; i<8; i++) 
			if(m_arrChilds[i])
				m_arrChilds[i]->FreeAreaBrushes(bRecursive);

	if(!m_plstAreaBrush)
		return;

	for(int i=0; i<m_plstAreaBrush->Count(); i++)
	{
		CBrush * pBrush = m_plstAreaBrush->GetAt(i);

		Get3DEngine()->UnRegisterEntity(pBrush);

		assert(!pBrush->GetEntityVisArea() && !pBrush->m_pOcNode);

		CStatObj * pAreaStatObj = (CStatObj *)pBrush->GetEntityStatObj(0);
		if(pAreaStatObj)
		{
			pBrush->SetEntityStatObj(0,0);
			IRenderMesh * pAreaLB = pAreaStatObj->GetRenderMesh();
			pAreaStatObj->SetRenderMesh(0);	
			GetRenderer()->DeleteRenderMesh(pAreaLB);
/*
			while(1)
			{
				CStatObj* tmp((CStatObj*)pAreaStatObj);
				int nFind = GetObjManager()->FindBrushType(tmp);
				if(nFind>=0)
					GetObjManager()->m_lstBrushTypes[nFind].pStatObj = NULL;
				else
					break;
			}
*/
			delete pAreaStatObj;
		}

		delete pBrush;
		pBrush = NULL;
	}

  for(int i=0; i<m_lstMergedObjects.Count(); i++)
    LinkObject(m_lstMergedObjects[i]);
	m_lstMergedObjects.Clear();

	SAFE_DELETE(m_plstAreaBrush);
	m_plstAreaBrush = NULL;

	m_bAreaMeshReady = false;
}

void COctreeNode::UpdateMergingTimeLimit() 
{ 
	if(m_fAreaMergingTimeLimit<0)
		PrintMessage("******* fMergingTimeLeft = %.3f ******* [%d]", m_fAreaMergingTimeLimit, GetFrameID());

	m_fAreaMergingTimeLimit = 0.001f*GetCVars()->e_scene_merging_max_time_ms;
}

void COctreeNode::UpdateTerrainNodes()
{
	m_pTerrainNode = GetTerrain()->FindMinNodeContainingBox(m_nodeBox);

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->UpdateTerrainNodes();
}

void COctreeNode::GetObjectsByType(PodArray<IRenderNode*> & lstObjects, EERType objType, AABB * pBBox)
{
	if(objType == eERType_Light && !m_bHasLights)
		return;

  if(pBBox && !Overlap::AABB_AABB(*pBBox, GetObjectsBBox()))
    return;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if(pObj->GetRenderNodeType() == objType)
      if(!pBBox || Overlap::AABB_AABB(*pBBox, pObj->GetBBox()))
			  lstObjects.Add(pObj);
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->GetObjectsByType(lstObjects, objType, pBBox);
}

void COctreeNode::CollectScatteringRenderNodes(PodArray<IRenderNode*>& RenderNodes) const
{
	for(IRenderNode* pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() != eERType_Brush && pObj->GetRenderNodeType() != eERType_Unknown && pObj->GetRenderNodeType() != eERType_VoxelMesh)
			continue;

		IMaterial* pMaterial = pObj->GetMaterial();
		if (NULL == pMaterial)
			continue;

		if (pMaterial->IsSubSurfScatterCaster())
			RenderNodes.Add(pObj);
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->CollectScatteringRenderNodes(RenderNodes);
}

bool COctreeNode::RayObjectsIntersection2D( Vec3 vStart, Vec3 vEnd, Vec3 & vClosestHitPoint, float & fClosestHitDistance, EERType eERType )
{
  FUNCTION_PROFILER_3DENGINE;

//	Vec3 vBoxHitPoint;
//	if(!Intersect::Ray_AABB(Ray(vStart, vEnd-vStart), m_objectsBox, vBoxHitPoint))
	//	return false;

	if(	vStart.x>m_objectsBox.max.x || vStart.y>m_objectsBox.max.y ||
			vStart.x<m_objectsBox.min.x || vStart.y<m_objectsBox.min.y )
				return false;

	if(!m_bCompiled)
		CompileObjects();

	float fOceanLevel = GetTerrain()->GetWaterLevel();

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		uint32 dwFlags = pObj->GetRndFlags();

		if(dwFlags&ERF_HIDDEN || !(dwFlags&ERF_CASTSHADOWMAPS) || dwFlags&ERF_PROCEDURAL)
			continue;

		if( pObj->GetRenderNodeType() != eERType )
			continue;

//		if(!Intersect::Ray_AABB(Ray(vStart, vEnd-vStart), pObj->GetBBox(), vBoxHitPoint))
	//		continue;

		const AABB & objBox = pObj->GetBBox();

		if((objBox.max.z-objBox.min.z) < 2.f)
			continue;

		if(objBox.max.z < fOceanLevel)
			continue;

		if(	vStart.x>objBox.max.x || vStart.y>objBox.max.y || vStart.x<objBox.min.x || vStart.y<objBox.min.y )
			continue;

		Matrix34 objMatrix;
		CStatObj * pStatObj = (CStatObj*)pObj->GetEntityStatObj(0, 0, &objMatrix);

		if(pStatObj->GetOcclusionAmount() < 0.32f)
			continue;

    {
      if(pStatObj->m_nFlags&STATIC_OBJECT_HIDDEN)
        continue;

      Matrix34 matInv = objMatrix.GetInverted();
      Vec3 vOSStart = matInv.TransformPoint(vStart);
      Vec3 vOSEnd = matInv.TransformPoint(vEnd);
      Vec3 vOSHitPoint(0,0,0), vOSHitNorm(0,0,0);

      Vec3 vBoxHitPoint;
      if(!Intersect::Ray_AABB(Ray(vOSStart, vOSEnd-vOSStart), pStatObj->GetAABB(), vBoxHitPoint))
        continue;

      vOSHitPoint = vOSStart;
      vOSHitPoint.z = pStatObj->GetObjectHeight(vOSStart.x,vOSStart.y);

      if(vOSHitPoint.z!=0)
      {
        Vec3 vHitPoint = objMatrix.TransformPoint(vOSHitPoint);
        float fDist = vHitPoint.GetDistance(vStart);
        if(fDist<fClosestHitDistance)
        {
          fClosestHitDistance = fDist;
          vClosestHitPoint = vHitPoint;
        }
      }
    }
	}

	for(int i=0; i<8; i++) 
		if(m_arrChilds[i])
			m_arrChilds[i]->RayObjectsIntersection2D( vStart, vEnd, vClosestHitPoint, fClosestHitDistance, eERType );

	return false;
}

bool COctreeNode::RayVoxelIntersection2D( Vec3 vStart, Vec3 vEnd, Vec3 & vClosestHitPoint, float & fClosestHitDistance )
{
  FUNCTION_PROFILER_3DENGINE;

  if(	!m_bHasVoxels ||
      vStart.x>m_objectsBox.max.x || vStart.y>m_objectsBox.max.y ||
      vStart.x<m_objectsBox.min.x || vStart.y<m_objectsBox.min.y )
    return false;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
  {
    if( pObj->GetRenderNodeType() != eERType_VoxelObject )
      continue;

    CVoxelObject* pVox = (CVoxelObject*)pObj;

    const AABB & objBox = pVox->GetBBox();

    if(	vStart.x>objBox.max.x || vStart.y>objBox.max.y || vStart.x<objBox.min.x || vStart.y<objBox.min.y )
      continue;

    Matrix34 objMatrix = pVox->GetMatrix();
    IRenderMesh * pRenderMesh = pVox->GetRenderMesh(0);
    if(!pRenderMesh)
      continue;

    Matrix34 matInv = objMatrix.GetInverted();
    Vec3 vOSStart = matInv.TransformPoint(vStart);
    Vec3 vOSEnd = matInv.TransformPoint(vEnd);

    Vec3 vOSHitPoint(0,0,0), vOSHitNorm(0,0,0);
    if(CObjManager::RayRenderMeshIntersection(pRenderMesh, vOSStart, vOSEnd-vOSStart, vOSHitPoint, vOSHitNorm, false, NULL))
    {
      Vec3 vHitPoint = objMatrix.TransformPoint(vOSHitPoint);
      float fDist = vHitPoint.GetDistance(vStart);
      if(fDist<fClosestHitDistance)
      {
        fClosestHitDistance = fDist;
        vClosestHitPoint = vHitPoint;
      }
    }
  }

  for(int i=0; i<8; i++) 
    if(m_arrChilds[i])
      m_arrChilds[i]->RayVoxelIntersection2D( vStart, vEnd, vClosestHitPoint, fClosestHitDistance );

  return false;
}

void COctreeNode::GenerateStatObjAndMatTables(std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
  {
    EERType eType = pObj->GetRenderNodeType();

    if( eType == eERType_Brush )
    {
      CBrush * pBrush = (CBrush *)pObj;
      if(CObjManager::GetItemId(pStatObjTable, pBrush->GetStatObj(), false)<0)
        pStatObjTable->push_back(pBrush->m_pStatObj);
    }

    if( eType == eERType_Brush || 
        eType == eERType_RoadObject_NEW ||
        eType == eERType_Decal ||
        eType == eERType_WaterVolume ||
        eType == eERType_DistanceCloud ||
        eType == eERType_WaterWave )
    {
      if(CObjManager::GetItemId(pMatTable, pObj->GetMaterial(), false)<0)
        pMatTable->push_back(pObj->GetMaterial());
    }
  }

  for(int i=0; i<8; i++) 
    if(m_arrChilds[i])
      m_arrChilds[i]->GenerateStatObjAndMatTables(pStatObjTable, pMatTable);
}

int COctreeNode::Cmp_OctreeNodeSize(const void* v1, const void* v2)
{
  COctreeNode *pNode1 = *((COctreeNode**)v1);
  COctreeNode *pNode2 = *((COctreeNode**)v2);

  if(pNode1->m_fNodeRadius > pNode2->m_fNodeRadius)
    return +1;
  if(pNode1->m_fNodeRadius < pNode2->m_fNodeRadius)
    return -1;

  return 0;
}

bool COctreeNode::IsEmpty()
{
  if( m_pParent )
  if( !m_lstObjects.m_pFirstNode )
  if( !m_lstOccluders.Count() && !m_lstMergedObjects.Count() )
  if( !m_arrChilds[0] && !m_arrChilds[1] && !m_arrChilds[2] && !m_arrChilds[3] )
  if( !m_arrChilds[4] && !m_arrChilds[5] && !m_arrChilds[6] && !m_arrChilds[7] )
    return true;

  return false;
}

void COctreeNode::ReleaseEmptyNodes()
{
  FUNCTION_PROFILER_3DENGINE;

  if(!m_arrEmptyNodes.Count())
    return;

  // sort childs first
  qsort(m_arrEmptyNodes.GetElements(), m_arrEmptyNodes.Count(), sizeof(m_arrEmptyNodes[0]), Cmp_OctreeNodeSize);

  int nInitCunt = m_arrEmptyNodes.Count();

  for(int i=0; i<nInitCunt && m_arrEmptyNodes.Count(); i++)
  {
    COctreeNode * pNode = m_arrEmptyNodes[0];

    // remove from list
    m_arrEmptyNodes.Delete(pNode);

    if( pNode->IsEmpty() )
    {
      COctreeNode * pParent = pNode->m_pParent;

      // unregister in parent
      for(int i=0; i<8; i++)
        if(pParent->m_arrChilds[i] == pNode)
          pParent->m_arrChilds[i] = NULL;

      delete pNode;

      // request parent validation
      if(pParent && pParent->IsEmpty() && m_arrEmptyNodes.Find(pParent)<0)
        m_arrEmptyNodes.Add(pParent);
    }
  }
}