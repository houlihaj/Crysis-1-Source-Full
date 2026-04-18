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

#include "DecalManager.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "Vegetation.h"
#include "terrain.h"
#include "VoxMan.h"


int CDecal::Update( bool & active, const float fFrameTime )
{
	// process life time and disable decal when needed
  m_fLifeTime -= fFrameTime;

  if(m_fLifeTime<0)
	{
    active=0;
		FreeRenderData();
	}	else if (m_ownerInfo.pRenderNode && m_ownerInfo.pRenderNode->m_nInternalFlags & IRenderNode::UPDATE_DECALS)
	{
		Matrix34 mtx;
		IStatObj *pStatObj = m_ownerInfo.GetOwner(mtx);
		phys_geometry *pPhysGeom;
		if (pStatObj && pStatObj->GetFlags() & (STATIC_OBJECT_CLONE|STATIC_OBJECT_GENERATED) && 
				(pPhysGeom=pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_DEFAULT)))
		{
			/*static IGeometry *g_pSphere=0;
			if (!g_pSphere)
			{
				primitives::sphere sph;
				sph.center.zero(); sph.r=1;
				g_pSphere = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
			}
			int i,j,ncont;
			geom_world_data gwd;
			intersection_params ip;
			ip.bExactBorder = 1;
			geom_contact *pcontact;
			gwd.offset = m_vPos;
			gwd.scale = m_fSize;
			if (ncont=pPhysGeom->pGeom->Intersect(g_pSphere,0,&gwd,&ip,pcontact))
			{
				WriteLockCond lock(*ip.plock,0); lock.SetActive();
				Vec3 n = m_vUp^m_vRight;
				for(i=0;i<ncont;i++) 
				{
					for(j=0;j<pcontact[i].nborderpt && 
							fabs_tpl(n*pPhysGeom->pGeom->GetNormal(pcontact[i].idxborder[j][0] & IDXMASK, pcontact[i].ptborder[j]))>0.85f; j++);
					if (j<pcontact[i].nborderpt) {
						active = false; break;
					}
				}
			}	else*/
				active = false;
			if (!active)
				FreeRenderData();
		}
		return 1;
	}
	return 0;
}

Vec3 CDecal::GetWorldPosition()
{
  Vec3 vPos = m_vPos;

  if(m_ownerInfo.pRenderNode)
  {
    if(m_eDecalType == eDecalType_OS_SimpleQuad || m_eDecalType == eDecalType_OS_OwnersVerticesUsed)
    {
      assert(m_ownerInfo.pRenderNode);
      if(m_ownerInfo.pRenderNode)
      {
        Matrix34 objMat;
        if(IStatObj * pEntObject = m_ownerInfo.GetOwner(objMat))
          vPos = objMat.TransformPoint(vPos);
        else if(m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_VoxelObject)
        {
          objMat = ((CVoxelObject*)m_ownerInfo.pRenderNode)->GetMatrix();
          vPos = objMat.TransformPoint(vPos);
        }
      }
    }
  }

  return vPos;
}

void CDecal::Render( const float fCurTime, int nAfterWater, uint nDLMask, float fDistanceFading)
{
	// Get decal alpha from life time
  float fAlpha = m_fLifeTime*2;

  if(fAlpha > 1.f)
    fAlpha = 1.f;
	else if(fAlpha<0)
		return;

  fAlpha *= fDistanceFading;

	float fSizeK;
	if(m_fGrowTime)
		fSizeK = min(1.f, cry_sqrtf((fCurTime - m_fLifeBeginTime)/m_fGrowTime));
	else
		fSizeK = 1.f;

	switch(m_eDecalType)
	{
		case eDecalType_WS_Merged:
		case eDecalType_OS_OwnersVerticesUsed:
		{
			assert(m_pRenderMesh && m_pRenderMesh->GetSysIndicesCount());

			if(!m_pRenderMesh)
				break;

			// setup transformation
			CRenderObject * pObj = GetRenderer()->EF_GetObject(true);
      if (!pObj)
        return;
			pObj->m_fSort = 0;

			Matrix34 objMat;
			if(m_ownerInfo.pRenderNode && !m_ownerInfo.GetOwner(objMat))
			{ 
        if(m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_VoxelObject)
        {
          CVoxelObject * pVox = (CVoxelObject*)m_ownerInfo.pRenderNode;
          objMat = pVox->GetMatrix();
        }
        else
        {
          assert(0); 
          return; 
        }
      }
			else
      if(!m_ownerInfo.pRenderNode)
				objMat.SetIdentity();

			pObj->m_II.m_Matrix = objMat;
			if(m_ownerInfo.pRenderNode)
				pObj->m_ObjFlags |= FOB_TRANS_MASK;

			pObj->m_DynLMMask = nDLMask;
			pObj->m_nRAEId = m_sortPrio;
			
			// somehow it's need's to be twice bigger to be same as simple decals
			float fSize2 = m_fSize*fSizeK*2.f;///m_ownerInfo.pRenderNode->GetScale();
			if(fSize2<0.0001f)
				return;

			// setup texgen
			// S component
			float correctScale( -1 ); 
			m_arrBigDecalRMCustomData[0] = correctScale * m_vUp.x/fSize2;
			m_arrBigDecalRMCustomData[1] = correctScale * m_vUp.y/fSize2;
			m_arrBigDecalRMCustomData[2] = correctScale * m_vUp.z/fSize2;
																				
			float D0 = 
				m_arrBigDecalRMCustomData[0]*m_vPos.x + 
				m_arrBigDecalRMCustomData[1]*m_vPos.y + 
				m_arrBigDecalRMCustomData[2]*m_vPos.z;

			m_arrBigDecalRMCustomData[3] = -D0+0.5f;

			// T component
			m_arrBigDecalRMCustomData[4] = m_vRight.x/fSize2;
			m_arrBigDecalRMCustomData[5] = m_vRight.y/fSize2;
			m_arrBigDecalRMCustomData[6] = m_vRight.z/fSize2;

			float D1 = 
				m_arrBigDecalRMCustomData[4]*m_vPos.x + 
				m_arrBigDecalRMCustomData[5]*m_vPos.y + 
				m_arrBigDecalRMCustomData[6]*m_vPos.z;

			m_arrBigDecalRMCustomData[7] = -D1+0.5f;

			// pass attenuation info
			m_arrBigDecalRMCustomData[8] = m_vPos.x;
			m_arrBigDecalRMCustomData[9] = m_vPos.y;
			m_arrBigDecalRMCustomData[10]= m_vPos.z;
			m_arrBigDecalRMCustomData[11]= m_fSize;

			// N component
			Vec3 vNormal( Vec3( correctScale * m_vUp ).Cross( m_vRight ).GetNormalized() );
			m_arrBigDecalRMCustomData[12] = vNormal.x*(m_fSize/m_fWSSize);
			m_arrBigDecalRMCustomData[13] = vNormal.y*(m_fSize/m_fWSSize);
			m_arrBigDecalRMCustomData[14] = vNormal.z*(m_fSize/m_fWSSize);
			m_arrBigDecalRMCustomData[15] = 0;

      CStatObj * pBody = NULL;
      bool bUseBending = GetCVars()->e_vegetation_bending != 0;
			if(m_ownerInfo.pRenderNode && m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation)
			{
        pObj->m_ObjFlags |= FOB_VEGETATION;
				CObjManager * pObjManager = GetObjManager();
				CVegetation * pVegetation = (CVegetation *)m_ownerInfo.pRenderNode;
				pBody = pObjManager->m_lstStaticTypes[pVegetation->m_nObjectTypeID].GetStatObj();
				assert(pObjManager && pVegetation && pBody);

				if(pObjManager && pVegetation && pBody) 
				{
          CRNTmpData *pRNTmpData = pVegetation->m_pRNTmpData;
          if( pRNTmpData && (pRNTmpData->userData.m_vFinalBending.x || pRNTmpData->userData.m_vFinalBending.y) && bUseBending)
          {
					  pBody->SetupBending(pObj, pRNTmpData->userData.m_vFinalBending);
          }
				}
        IMaterial *pMat = m_ownerInfo.pRenderNode->GetMaterial();
        pMat = pMat->GetSubMtl(m_ownerInfo.nMatID);
        if (pMat)
        {
          // Support for public parameters from owner (deformations)
          SShaderItem& SH = pMat->GetShaderItem();
          if (SH.m_pShaderResources)
          {
            IMaterial *pMatDecal = m_pRenderMesh->GetMaterial();
            SShaderItem& SHDecal = pMatDecal->GetShaderItem();
            if (bUseBending)
            {
              pObj->m_nMDV = SH.m_pShader->GetVertexModificator();
              if (SHDecal.m_pShaderResources && pObj->m_nMDV)
                SH.m_pShaderResources->ExportModificators(SHDecal.m_pShaderResources, pObj);
            }
          }
        }
        if (m_eDecalType == eDecalType_OS_OwnersVerticesUsed)
          pObj->m_ObjFlags |= FOB_OWNER_GEOMETRY;
        pObj->m_pCharInstance = pVegetation->m_pFoliage;
        if (pBody)
        {
          PodArray<CRenderChunk>* pChunks = m_pRenderMesh->GetChunks();
          if (pChunks && pChunks->size())
            (*pChunks)[0].m_arrChunkBoneIDs.Clear();
          m_pRenderMesh->SetVertexContainer(pBody->m_pRenderMesh);
          if (pObj->m_pCharInstance)
          {
            // Support for skinning
            PodArray<CRenderChunk> *pCS = pBody->m_pRenderMesh->GetChunksSkinned();
            if (pCS)
            {
              for (int i=0; i<pCS->size(); i++)
              {
                CRenderChunk *pCH = &(*pCS)[i];
                if (pCH->m_nMatID == m_ownerInfo.nMatID)
                {
                  (*pChunks)[0].m_arrChunkBoneIDs.AddList(pCH->m_arrChunkBoneIDs.begin(), pCH->m_arrChunkBoneIDs.size());
                  break;
                }
              }
            }
          }
        }
			}

			// draw complex decal using new indices and original object vertices
			pObj->m_fAlpha = fAlpha;
			pObj->m_ObjFlags |= FOB_DECAL | FOB_DECAL_TEXGEN_3D | FOB_NO_Z_PASS | FOB_INSHADOW;
			pObj->m_nTextureID = -1;	
			pObj->m_nTextureID1 = -1;	
			pObj->m_II.m_AmbColor = m_vAmbient;
			if (GetCVars()->e_decals_scissor)
			{
				Matrix44 proj;
				Matrix44 view;
				GetRenderer()->GetModelViewMatrix(view.GetData());
				GetRenderer()->GetProjectionMatrix(proj.GetData());
				int screen_width = GetRenderer()->GetWidth();
				int screen_height = GetRenderer()->GetHeight();
				Vec4 viewspace_pos = Vec4(m_vWSPos, 1) * view;
				float corner1x = viewspace_pos.x - 1.5f*m_fWSSize;
				float corner1y = viewspace_pos.y + 1.5f*m_fWSSize;
				float corner2x = viewspace_pos.x + 1.5f*m_fWSSize;
				float corner2y = viewspace_pos.y - 1.5f*m_fWSSize;
				float w = 0.5f / viewspace_pos.z*proj.m23;
				float scissor_minx = (corner1x*proj.m00) * w;
				float scissor_miny = -(corner1y*proj.m11) * w;
				float scissor_maxx = (corner2x*proj.m00) * w;
				float scissor_maxy = -(corner2y*proj.m11) * w;

				pObj->m_nScissorX1 = max(min(int(screen_width * (scissor_minx + 0.5f)), screen_width), 0);
				pObj->m_nScissorX2 = max(min(int(screen_width * (scissor_maxx + 0.5f)), screen_width), 0);
				pObj->m_nScissorY1 = max(min(int(screen_height * (scissor_miny + 0.5f)), screen_height), 0);
				pObj->m_nScissorY2 = max(min(int(screen_height * (scissor_maxy + 0.5f)), screen_height), 0);
				if (pObj->m_nScissorX1 >= pObj->m_nScissorX2 || pObj->m_nScissorY1 >= pObj->m_nScissorY2)
					pObj->m_nScissorX1 = pObj->m_nScissorX2 = pObj->m_nScissorY1 = pObj->m_nScissorY2 = 0;
			}
			else
				pObj->m_nScissorX1 = pObj->m_nScissorX2 = pObj->m_nScissorY1 = pObj->m_nScissorY2 = 0;

			m_pRenderMesh->SetRECustomData(m_arrBigDecalRMCustomData, 0, fAlpha);
      m_pRenderMesh->AddRenderElements(pObj, EFSLIST_GENERAL, nAfterWater);
		}
		break;

		case eDecalType_OS_SimpleQuad:
		{ 
			assert(m_ownerInfo.pRenderNode);
			if(!m_ownerInfo.pRenderNode)
				break;

			// transform decal in software from owner space into world space and render as quad
			Matrix34 objMat;
			IStatObj * pEntObject = m_ownerInfo.GetOwner(objMat);
			if(!pEntObject)
				break;

			Vec3 vPos	 = objMat.TransformPoint(m_vPos);
			Vec3 vRight = objMat.TransformVector(m_vRight*m_fSize);
			Vec3 vUp    = objMat.TransformVector(m_vUp*m_fSize);
			UCol uCol; 
			
			uCol.dcolor = 0xffffffff; 
			uCol.bcolor[3] = fastround_positive(fAlpha*255);

			GetObjManager()->AddDecalToRenderer( m_pMaterial, nDLMask, m_sortPrio, vRight * fSizeK, vUp * fSizeK, uCol, 
				ParticleBlendType_AlphaBased, m_vAmbient, vPos, nAfterWater,
				m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation ? (CVegetation*) m_ownerInfo.pRenderNode : 0 );
		}
		break;

		case eDecalType_WS_SimpleQuad:
		{	// draw small world space decal untransformed
			UCol uCol; 
			uCol.dcolor = 0;
			uCol.bcolor[3] = fastround_positive(fAlpha*255);

      GetObjManager()->AddDecalToRenderer( m_pMaterial, nDLMask, m_sortPrio, m_vRight * m_fSize * fSizeK, 
				m_vUp * m_fSize * fSizeK, uCol, ParticleBlendType_AlphaBased, m_vAmbient, m_vPos, nAfterWater );
		}
		break;

		case eDecalType_WS_OnTheGround:
		{
			RenderBigDecalOnTerrain(fAlpha, fSizeK, nDLMask);
		}
		break;
	}
}

void CDecal::FreeRenderData()
{
	// delete render mesh
	GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
	m_pRenderMesh = NULL;

	m_ownerInfo.pRenderNode=0;
}
/*
void CDecal::Render3DObject()
{
  // draw
  if(m_pStatObj)
  {
  	Ang3 vAngles = Ang3(m_vFront);
    vAngles.x+=90;
		Matrix34 mat = Matrix34::CreateRotationXYZ( DEG2RAD(vAngles), m_vPos );

		Matrix34 objMat;
		if(IStatObj * pEntObject = m_ownerInfo.GetOwner(objMat))
			mat = objMat*mat;

    SRendParams rParms;
    rParms.pMatrix = &mat;
    rParms.dwFObjFlags |= FOB_TRANS_MASK;
    m_pStatObj->Render(rParms);
  }
}
*/
void CDecal::RenderBigDecalOnTerrain(float fAlpha, float fScale, uint nDLMask)
{
	Vec3 vPos = m_vPos;
	float fRadius = m_fSize * fScale;

	// check terrain bounds
	if(vPos.x < -fRadius || vPos.y < -fRadius)
		return;
	if(vPos.x >= CTerrain::GetTerrainSize() + fRadius || vPos.y >= CTerrain::GetTerrainSize() + fRadius)
		return;

	const int nUsintSize = CTerrain::GetHeightMapUnitSize();
	fRadius += nUsintSize;
	vPos.x  = float(int(vPos.x  + 0.5f*nUsintSize)/nUsintSize*nUsintSize);
	vPos.y  = float(int(vPos.y  + 0.5f*nUsintSize)/nUsintSize*nUsintSize);
	vPos.z  = float(int(vPos.z  + 0.5f*nUsintSize)/nUsintSize*nUsintSize);
	fRadius = float(int(fRadius + 0.5f*nUsintSize)/nUsintSize*nUsintSize);

	if( fabs(vPos.z - Get3DEngine()->GetTerrainZ(int(vPos.x),int(vPos.y))) > fRadius )
		return; // too far from ground surface

	// setup texgen
	float fSize2 = m_fSize*fScale*2.f;
	if(fSize2<0.05f)
		return;

	// S component
	float correctScale( -1 );
	m_arrBigDecalRMCustomData[0] = correctScale * m_vUp.x/fSize2;
	m_arrBigDecalRMCustomData[1] = correctScale * m_vUp.y/fSize2;
	m_arrBigDecalRMCustomData[2] = correctScale * m_vUp.z/fSize2;

	float D0 = 
		m_arrBigDecalRMCustomData[0]*m_vPos.x + 
		m_arrBigDecalRMCustomData[1]*m_vPos.y + 
		m_arrBigDecalRMCustomData[2]*m_vPos.z;

	m_arrBigDecalRMCustomData[3] = -D0+0.5f;

	// T component
	m_arrBigDecalRMCustomData[4] = m_vRight.x/fSize2;
	m_arrBigDecalRMCustomData[5] = m_vRight.y/fSize2;
	m_arrBigDecalRMCustomData[6] = m_vRight.z/fSize2;

	float D1 = 
		m_arrBigDecalRMCustomData[4]*m_vPos.x + 
		m_arrBigDecalRMCustomData[5]*m_vPos.y + 
		m_arrBigDecalRMCustomData[6]*m_vPos.z;

	m_arrBigDecalRMCustomData[7] = -D1+0.5f;

	// pass attenuation info
	m_arrBigDecalRMCustomData[8] = m_vPos.x;
	m_arrBigDecalRMCustomData[9] = m_vPos.y;
	m_arrBigDecalRMCustomData[10]= m_vPos.z;
	m_arrBigDecalRMCustomData[11]= fSize2;

	Vec3 vNormal( Vec3( correctScale * m_vUp ).Cross( m_vRight ).GetNormalized() );
	m_arrBigDecalRMCustomData[12] = vNormal.x;
	m_arrBigDecalRMCustomData[13] = vNormal.y;
	m_arrBigDecalRMCustomData[14] = vNormal.z;
	m_arrBigDecalRMCustomData[15] = 0;

	CRenderObject * pObj = GetIdentityCRenderObject();
  if (!pObj)
    return;
	pObj->m_fAlpha = fAlpha;
	pObj->m_ObjFlags |= FOB_DECAL | FOB_DECAL_TEXGEN_2D | FOB_NO_Z_PASS | FOB_INSHADOW;
	pObj->m_nTextureID = -1;
	pObj->m_nTextureID1 = -1;
	pObj->m_II.m_AmbColor = m_vAmbient;

	pObj->m_nRAEId = m_sortPrio;

  pObj->m_DynLMMask = nDLMask;

  Plane planes[4];
  Vec3 vRight = m_vRight.GetNormalized();
  float fRotationFactor = 1.f - fabs ( fabs(vRight.x) - fabs(vRight.y) );
  float fClipRadius = m_fSize*(1.f+fRotationFactor*0.42f);
  planes[0].SetPlane( Vec3( 1, 0, 0), Vec3( fClipRadius, 0, 0) + m_vPos);
  planes[1].SetPlane( Vec3(-1, 0, 0), Vec3(-fClipRadius, 0, 0) + m_vPos);
  planes[2].SetPlane( Vec3( 0, 1, 0), Vec3( 0, fClipRadius, 0) + m_vPos);
  planes[3].SetPlane( Vec3( 0,-1, 0), Vec3( 0,-fClipRadius, 0) + m_vPos);

	// m_pRenderMesh might get updated by the following function
	GetTerrain()->RenderArea( vPos, fRadius, &m_pRenderMesh,
    pObj, m_pMaterial, "BigDecalOnTerrain", m_arrBigDecalRMCustomData, GetCVars()->e_decals_clip ? planes : NULL);
}