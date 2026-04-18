////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   PhysCallbacks.cpp
//  Created:     14/11/2006 by Anton.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PhysCallbacks.h"
#include "RenderMeshUtils.h"
#include "Brush.h"

#define RENDER_MESH_COLLISION_IGNORE_DISTANCE 30.0f
#ifndef RENDER_MESH_TEST_DISTANCE
	#define RENDER_MESH_TEST_DISTANCE (0.2f)
#endif

CPhysStreamer g_PhysStreamer;

void CPhysCallbacks::Init()
{
	GetPhysicalWorld()->AddEventClient(EventPhysBBoxOverlap::id, &CPhysCallbacks::OnFoliageTouched, 1);
	GetPhysicalWorld()->AddEventClient(EventPhysCollision::id, &CPhysCallbacks::OnPhysCollision, 1, 2000.0f);
	GetPhysicalWorld()->AddEventClient(EventPhysStateChange::id, &CPhysCallbacks::OnPhysStateChange, 1);
	GetPhysicalWorld()->SetPhysicsStreamer(&g_PhysStreamer);
}

//////////////////////////////////////////////////////////////////////////
void CPhysCallbacks::Done()
{
	GetPhysicalWorld()->RemoveEventClient(EventPhysBBoxOverlap::id, &CPhysCallbacks::OnFoliageTouched, 1);
	GetPhysicalWorld()->RemoveEventClient(EventPhysCollision::id, &CPhysCallbacks::OnPhysCollision, 1);
	GetPhysicalWorld()->RemoveEventClient(EventPhysStateChange::id, &CPhysCallbacks::OnPhysStateChange, 1);
	GetPhysicalWorld()->SetPhysicsStreamer(0);
}

//////////////////////////////////////////////////////////////////////////
int CPhysCallbacks::OnFoliageTouched( const EventPhys *pEvent )
{
	EventPhysBBoxOverlap *pOverlap = (EventPhysBBoxOverlap*)pEvent;
	if (pOverlap->iForeignData[1]==PHYS_FOREIGN_ID_STATIC)
	{
		pe_params_bbox pbb;
		pOverlap->pEntity[1]->GetParams(&pbb);
		pe_params_part pp;
		pe_status_pos sp;
		pe_status_nparts snp;
		primitives::box bbox;
		OBB obb;
		Vec3 sz = pbb.BBox[1]-pbb.BBox[0];
		int nWheels=0,nParts=pOverlap->pEntity[0]->GetStatus(&snp);
		if (pOverlap->pEntity[0]->GetType()==PE_WHEELEDVEHICLE)
		{
			pe_params_car pc; pOverlap->pEntity[0]->GetParams(&pc);
			nWheels = pc.nWheels;
		}

		pOverlap->pEntity[0]->GetStatus(&sp);
		for(pp.ipart=0; pp.ipart<nParts-nWheels && pOverlap->pEntity[0]->GetParams(&pp); pp.ipart++,MARK_UNUSED pp.partid) 
			if (pp.flagsOR & (geom_colltype0|geom_colltype1|geom_colltype2|geom_colltype3))
			{
				pp.pPhysGeomProxy->pGeom->GetBBox(&bbox);
				obb.SetOBB(Matrix33(sp.q*pp.q)*bbox.Basis.T(), bbox.size*pp.scale, Vec3(ZERO));
				if (Overlap::AABB_OBB(AABB(pbb.BBox[0],pbb.BBox[1]), sp.q*(pp.q*bbox.center*pp.scale+pp.pos)+sp.pos, obb))
					goto foundbox;
			}
			return 1;
foundbox:

			int i;
			pe_params_foreign_data pfd;
			C3DEngine *pEngine = (C3DEngine*)gEnv->p3DEngine;
			pOverlap->pEntity[0]->GetParams(&pfd);
			if ((i=(pfd.iForeignFlags>>8&255)-1)<0)
			{
				if ((i=pEngine->m_arrEntsInFoliage.size())<255)
				{
					SEntInFoliage eif;
					eif.id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pOverlap->pEntity[0]);
					eif.timeIdle = 0;
					pEngine->m_arrEntsInFoliage.push_back(eif);
					MARK_UNUSED pfd.pForeignData,pfd.iForeignData;
					pfd.iForeignFlags |= (i+1)<<8;
					pOverlap->pEntity[0]->SetParams(&pfd,1);
				}
			}	else
				pEngine->m_arrEntsInFoliage[i].timeIdle = 0;

			IRenderNode *pVeg = (IRenderNode*)pOverlap->pForeignData[1];
			CCamera &cam = gEnv->pSystem->GetViewCamera();
			int cullDist = GetCVars()->e_cull_veg_activation;
			if (!cullDist || pVeg->GetDrawFrame()+10 > gEnv->pRenderer->GetFrameID() && 
					(cam.GetPosition()-pVeg->GetPos()).len2()*sqr(cam.GetFov()) < sqr(cullDist*1.04f))
				if (pVeg->GetPhysics()==pOverlap->pEntity[1] && !pVeg->PhysicalizeFoliage() && nWheels>0 && sz.x*sz.y<0.5f)
					for(pp.ipart=nParts-1; pp.ipart>=nParts-nWheels && pOverlap->pEntity[0]->GetParams(&pp); pp.ipart--,MARK_UNUSED pp.partid)
						if (pp.pPhysGeomProxy->pGeom->PointInsideStatus(
							(((pbb.BBox[1]+pbb.BBox[0])*0.5f-sp.pos)*sp.q-pp.pos)*pp.q*(pp.scale==1.0f ? 1.0f:1.0f/pp.scale)))
						{ // break the vegetation object
							CStatObj *pStatObj = (CStatObj*)pVeg->GetEntityStatObj(0);
							if (pStatObj)
							{
								IParticleEffect *pDisappearEffect = pStatObj->GetSurfaceBreakageEffect( SURFACE_BREAKAGE_TYPE("destroy") );
								if (pDisappearEffect)
									pDisappearEffect->Spawn(true, IParticleEffect::ParticleLoc(pVeg->GetBBox().GetCenter(),Vec3(0,0,1),(sz.x+sz.y+sz.z)*0.3f));
							}
							pVeg->Dephysicalize();
							pEngine->m_pObjectsTree->DeleteObject(pVeg);
							break;
						}
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysCallbacks::OnPhysStateChange(const EventPhys *pEvent)
{
	EventPhysStateChange *pepsc = (EventPhysStateChange*)pEvent;
	int i;
	pe_params_foreign_data pfd;

	if (pepsc->iSimClass[0]==2 && pepsc->iSimClass[1]==1 && pepsc->pEntity->GetParams(&pfd) && (i=(pfd.iForeignFlags>>8)-1)>=0)
	{
		((C3DEngine*)gEnv->p3DEngine)->RemoveEntInFoliage(i);
		pe_params_bbox pbb;
		IPhysicalEntity *pentlist[8],**pents=pentlist;
		IRenderNode *pBrush;
		pepsc->pEntity->GetParams(&pbb);
		for(i=gEnv->pPhysicalWorld->GetEntitiesInBox(pbb.BBox[0],pbb.BBox[1],pents,ent_static|ent_allocate_list,6)-1; i>=0; i--)
			if (pBrush = (IRenderNode*)pents[i]->GetForeignData(PHYS_FOREIGN_ID_STATIC))
				pBrush->PhysicalizeFoliage(true,2);
		if (pents!=pentlist)
			gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(pents);
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysCallbacks::OnPhysCollision(const EventPhys *pEvent)
{
	EventPhysCollision *pCollision = (EventPhysCollision*)pEvent;

	if (pCollision->pEntity[0]->GetType() == PE_PARTICLE)
	{
		if (pCollision->iForeignData[1]==PHYS_FOREIGN_ID_STATIC)
		{
			ISurfaceType *pSrfType = GetMatMan()->GetSurfaceType( pCollision->idmat[1] );
			if (pSrfType && pSrfType->GetFlags() & SURFACE_TYPE_NO_COLLIDE)
			{
				FRAME_PROFILER( "3dEngine::RaytraceVegetation", GetISystem(), PROFILE_3DENGINE );

				IRenderNode *pVeg = (IRenderNode*)pCollision->pForeignData[1];
				CCamera &cam = gEnv->pSystem->GetViewCamera();
				int cullDist = GetCVars()->e_cull_veg_activation;
				if (cullDist && (pVeg->GetDrawFrame()+10 < gEnv->pRenderer->GetFrameID() ||
						(cam.GetPosition()-pVeg->GetPos()).len2()*sqr(cam.GetFov()) > sqr(cullDist*1.04f)))
					return 0;

				int i,j,bHit=0;
				float tx,ty,len2,denom2;
				Matrix34 mtx,mtxInv;
				Vec3 *pVtx,pt,pt0,dir,pthit,nup,edge,axisx,axisy;
				bool bLocal;
				pe_action_impulse ai;
				pe_status_rope sr;
				CStatObj *pStatObj = (CStatObj*)pVeg->GetEntityStatObj(0, 0, &mtx);
				if (!pStatObj)
          return (pVeg->GetRenderNodeType() == eERType_VoxelObject);

				if (pStatObj->m_nSpines && pCollision->n*pCollision->vloc[0]>0) 
				{
					ai.impulse = pCollision->vloc[0]*(pCollision->mass[0]*0.5f);
					pt=pt0=pCollision->pt; dir=pCollision->vloc[0].normalized();
					if (bLocal=(pVeg->GetFoliage()==0))	
					{	// if branches are not physicalized, operate in local space
						mtxInv = mtx.GetInverted();
						pt=mtxInv.TransformPoint(pt); dir=mtxInv.TransformVector(dir);
					}
					for(i=0;i<pStatObj->m_nSpines;i++) if (pStatObj->m_pSpines[i].bActive)
					{
						nup = pStatObj->m_pSpines[i].navg;
						if (bLocal)
							pVtx = pStatObj->m_pSpines[i].pVtx;
						else {
							pVtx = sr.pPoints = pStatObj->m_pSpines[i].pVtxCur;
							if (!pVeg->GetBranchPhys(i))
								continue;
							pVeg->GetBranchPhys(i)->GetStatus(&sr);
							nup = mtx.TransformVector(nup);
						}
						//axisx = pVtx[pStatObj->m_pSpines[i].nVtx-1]-pVtx[0]; axisy = pt-pVtx[0];
						//if (sqr_signed(axisx*axisy) < 0)//axisx.len2()*axisy.len2()*0.25f)
						//	continue;
						for(j=0;j<pStatObj->m_pSpines[i].nVtx-1;j++)
						{
							edge = pVtx[j+1]-pVtx[j]; len2=edge.len2();
							axisx=edge^nup; denom2=axisx.len2(); axisy=axisx^edge;
							tx = (pVtx[j]-pt)*axisx; ty = dir*axisx;
							pthit = (pt-pVtx[j])*ty + dir*tx;
							if (inrange(pthit*edge, 0.0f,len2*ty) && inrange(sqr_signed(pthit*axisy), 
								sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].z*ty)*denom2, sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].w*ty)*denom2))
								break; // hit y fin
							tx = (pVtx[j]-pt)*axisy; ty = dir*axisy;
							pthit = (pt-pVtx[j])*ty + dir*tx;
							if (inrange(pthit*edge, 0.0f,len2*ty) && inrange(sqr_signed(pthit*axisx), 
								sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].x*ty)*denom2, sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].y*ty)*denom2))
								break; // hit x fin
						}
						if (j<pStatObj->m_pSpines[i].nVtx-1 && pVeg->PhysicalizeFoliage(true,1))
						{
							pthit = pthit/ty+pVtx[j];
							if (bLocal)
								pthit = mtx*pthit; 
							if ((pthit-pt0)*pCollision->vloc[0] < (pCollision->pt-pt0)*pCollision->vloc[0])
								pCollision->pt = pthit;
							ai.point = pthit;
							ai.partid = j;
							IPhysicalEntity *pent = pCollision->pEntity[1];
							(pCollision->pEntity[1] = pVeg->GetBranchPhys(i))->Action(&ai);
							pCollision->idmat[1] = pStatObj->m_pSpines[i].idmat;
							((CStatObjFoliage*)pCollision->pEntity[1]->GetForeignData(PHYS_FOREIGN_ID_FOLIAGE))->OnHit(pCollision);
							pCollision->pEntity[1] = pent;
							bHit = 1;
						}
					}
					return bHit;
				} else if (!pStatObj->m_nSpines && pCollision->n*pCollision->vloc[0]<0) 
				{
					if (((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves<0)
					{
						ISurfaceType *pLeavesMat = GetMatMan()->GetSurfaceTypeByName("mat_leaves","Physical Collision");
						((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves = pLeavesMat ? pLeavesMat->GetId() : 0;
					}
					pCollision->idmat[1] = ((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves;
					return 1;
				}
			}
		}

		if (pCollision->iForeignData[0] == PHYS_FOREIGN_ID_ENTITY)
		{
			if (!pCollision->vloc[0].IsZero())
			{
				bool bPierceable;
				// If this is collision with some entity particle (probably bullet,grenade or rocket).
				if (!TestCollisionWithRenderMesh( pCollision, &bPierceable ))
				{
					if (bPierceable)
						return 0; // don't do any backtracking if the hit was already pierceable

					// Let bullet fly some more.
					pe_action_set_velocity actionVelocity;
					actionVelocity.v = pCollision->vloc[0];
					pCollision->pEntity[0]->Action(&actionVelocity);

					//pe_params_particle paramsPart;
					//pCollision->pEntity[0]->GetParams(&paramsPart);

					pe_params_pos ppos;
					ppos.pos = pCollision->pt + pCollision->vloc[0].GetNormalized()*0.003f;//(paramsPart.size + 0.01f);
					pCollision->pEntity[0]->SetParams(&ppos);

					return 0;
				}
			}
		}

		if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ROPE && pCollision->vloc[0].len2()>sqr(20.0f) && !gEnv->bMultiplayer)
		{
			IRopeRenderNode *pRopeNode = (IRopeRenderNode*)pCollision->pForeignData[1];
			IEntity *pRopeEntity = gEnv->pEntitySystem->GetEntity((EntityId)pRopeNode->GetEntityOwner());

			pe_params_rope pr;
			pr.pEntTiedTo[1] = 0;
			pCollision->pEntity[1]->SetParams(&pr);

			if (pRopeEntity)
			{
				int npt = pRopeNode->GetPointsCount();
				const Vec3 *pt = pRopeNode->GetPoints();
				const Matrix34 &tm = pRopeEntity->GetWorldTM();
				EventPhysJointBroken epjb;
				epjb.idJoint=0; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
				epjb.pEntity[0]=epjb.pEntity[1] = pCollision->pEntity[1]; 
				epjb.pForeignData[0]=epjb.pForeignData[1] = pCollision->pForeignData[1]; 
				epjb.iForeignData[0]=epjb.iForeignData[1] = pCollision->iForeignData[1];
				epjb.pt = tm.TransformPoint(pt[npt-1]); 
				epjb.n = tm.TransformVector((pt[npt-1]-pt[npt-2]).normalized());
				epjb.partid[0] = 1;
				epjb.partid[1] = -1;
				epjb.partmat[0]=epjb.partmat[1] = -1;

				SEntityEvent evt;
				evt.event = ENTITY_EVENT_PHYS_BREAK;
				evt.nParam[0] = (INT_PTR)&epjb;
				evt.nParam[1] = 0;
				pRopeEntity->SendEvent(evt);
			}
		}
	}

	if (pCollision->iForeignData[1]==PHYS_FOREIGN_ID_FOLIAGE)
	{
		((CStatObjFoliage*)pCollision->pForeignData[1])->m_timeIdle = 0;
		((CStatObjFoliage*)pCollision->pForeignData[1])->OnHit(pCollision);
	}
	
	return 1;
}

//////////////////////////////////////////////////////////////////////////
inline IStatObj* GetIStatObjFromPhysicalEntity( IPhysicalEntity *pent,int nPartId,Matrix34 *pTM,int &nMats,int* &pMatMapping )
{
	pe_params_part ppart;
	ppart.partid  = nPartId;
	if (pTM)
		ppart.pMtx3x4 = pTM;
	if (pent->GetParams( &ppart ))
	{
		if (ppart.pPhysGeom && ppart.pPhysGeom->pGeom)
		{
			void *ptr = ppart.pPhysGeom->pGeom->GetForeignData(0);
			if (ppart.pPhysGeom->pMatMapping == ppart.pMatMapping)
			{
				// No custom material.
				nMats = 0;
				pMatMapping = 0;
			}
			else
			{
				nMats = ppart.nMats;
				pMatMapping = ppart.pMatMapping;
			}
			return (IStatObj*)ptr;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CPhysCallbacks::TestCollisionWithRenderMesh( EventPhysCollision *pCollisionEvent, bool *pbPierceable )
{
	// Ignore it if too far.
	Vec3 vCamPos = GetSystem()->GetViewCamera().GetPosition();

	// do not spawn if too far
	float fZoom = GetObjManager()->m_fZoomFactor;
	float fCollisionDistance = pCollisionEvent->pt.GetDistance(vCamPos);
	if (fCollisionDistance*fZoom > GetCVars()->e_phys_bullet_coll_dist)
	{
		return true;
	}

	// Check if backface collision.
	if (pCollisionEvent->vloc[0].Dot(pCollisionEvent->n) > 0)
	{
		// Ignore testing render-mesh for backface collisions.
		return false;
	}

	int nMats = 0;
	int *pMatMapping = 0;
	// Get IStatObj from physical entity.
	Matrix34 partTM;
	IStatObj *pStatObj = GetIStatObjFromPhysicalEntity( pCollisionEvent->pEntity[1],pCollisionEvent->partid[1],&partTM,nMats,pMatMapping );
	bool bPierceable=false;
	if (pCollisionEvent && pCollisionEvent->pEntity[0])
	{
		float dummy;
		unsigned int pierceability0,pierceability1;
		if (pCollisionEvent->pEntity[0]->GetType()==PE_PARTICLE)
			pierceability0 = pCollisionEvent->partid[0];
		else
			GetPhysicalWorld()->GetSurfaceParameters(pCollisionEvent->idmat[0], dummy,dummy, pierceability0);
		GetPhysicalWorld()->GetSurfaceParameters(pCollisionEvent->idmat[1], dummy,dummy, pierceability1);
		bPierceable = (pierceability0&15) < (pierceability1&15);
	}
	if (pbPierceable)
		*pbPierceable = bPierceable;

  if(pStatObj)
    if(int nMinLod = ((CStatObj*)pStatObj)->GetMinUsableLod())
      if(IStatObj *pLodObj = pStatObj->GetLodObject(nMinLod))
        pStatObj = pLodObj;

	if (pStatObj)
	{
		IRenderMesh *pRenderMesh = pStatObj->GetRenderMesh();
		if (pRenderMesh)
		{
			Matrix34 worldTM;
			pe_status_pos ppos;
			ppos.pMtx3x4 = &worldTM;
			pCollisionEvent->pEntity[1]->GetStatus(&ppos);
			worldTM = worldTM * partTM;
			Matrix33 worldRot = Matrix33(worldTM);
			worldRot /= worldRot.GetColumn(0).GetLength();
			Vec3 dir = pCollisionEvent->vloc[0].GetNormalized();

			// transform decal into object space 
			Matrix34 worldTM_Inverted = worldTM.GetInverted();

			// put hit normal into the object space
			Vec3 vRayDir = dir*worldRot;

			// put hit position into the object space
			Vec3 vHitPos = worldTM_Inverted.TransformPoint(pCollisionEvent->pt);

			Vec3 vLineP1 = vHitPos - vRayDir*RENDER_MESH_TEST_DISTANCE;
		
			SRayHitInfo hitInfo;
			memset(&hitInfo,0,sizeof(hitInfo));
			hitInfo.inReferencePoint = vHitPos;
			hitInfo.inRay.origin = vLineP1;
			hitInfo.inRay.direction = vRayDir;
			hitInfo.bInFirstHit = false;
      hitInfo.bUseCache = CStatObj::GetCVars()->e_decals_hit_cache!=0;

			//CryLogAlways( "Do Test %f,%f",vHitPos.x,vHitPos.y );
			if (CRenderMeshUtils::RayIntersection( pRenderMesh,hitInfo ))
			{
				Vec3 ptnew = worldTM.TransformPoint(hitInfo.vHitPos), ptlim;
				if (bPierceable)
				{
					if (pCollisionEvent->next && pCollisionEvent->next->idval==EventPhysCollision::id && 
							((EventPhysCollision*)pCollisionEvent->next)->pEntity[0]==pCollisionEvent->pEntity[0])
						ptlim = ((EventPhysCollision*)pCollisionEvent->next)->pt;
					else {
						pe_status_pos sp;
						pCollisionEvent->pEntity[0]->GetStatus(&sp);
						ptlim = pCollisionEvent->pt + dir*(0.01f + max(max(0.0f, dir*(sp.pos-pCollisionEvent->pt)),
							dir*pCollisionEvent->vloc[0]*(GetTimer()->GetFrameTime()*1.1f)));
					}
					if ((ptnew-pCollisionEvent->pt)*pCollisionEvent->vloc[0] > (ptlim-pCollisionEvent->pt)*pCollisionEvent->vloc[0])
						return false; // don't generate hits ahead of the physical particle or its next reported hit
				}

				pCollisionEvent->pt = ptnew;
				pCollisionEvent->n = worldRot.TransformVector(hitInfo.vHitNormal);
				if (pMatMapping)
				{
					// When using custom material mapping.
					if (hitInfo.nHitMatID < nMats)
					{
						hitInfo.nHitSurfaceID = pMatMapping[hitInfo.nHitMatID];
					}
				}
				if (hitInfo.nHitSurfaceID)
					pCollisionEvent->idmat[1] = hitInfo.nHitSurfaceID;
			}
			else
			{
				// Let bullet fly some more.
				return false;
			}
		}
	}

	return true;
}


bool C3DEngine::RefineRayHit(ray_hit *phit, const Vec3 &dir)
{
	EventPhysCollision epc; memset(&epc,0,sizeof(epc));
	epc.pt=phit->pt; epc.n=phit->n; epc.vloc[0]=dir; 
	epc.pEntity[1]=phit->pCollider;	epc.partid[1]=phit->partid;
	if (CPhysCallbacks::TestCollisionWithRenderMesh(&epc))
	{
		phit->pt=epc.pt; phit->n=epc.n;
		return true;
	}
	return false;
}


int CPhysStreamer::CreatePhysicalEntity(void *pForeignData,int iForeignData,int iForeignFlags)
{
	if (iForeignData==PHYS_FOREIGN_ID_STATIC) 
		if (iForeignFlags & (PFF_BRUSH|PFF_VEGETATION))
			((IRenderNode*)pForeignData)->Physicalize(true);
		else
			return 0;
	return 1;
}

int CPhysStreamer::CreatePhysicalEntitiesInBox(const Vec3 &boxMin, const Vec3 &boxMax)
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_3DENGINE );
	if (((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree)
	((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree->PhysicalizeVegetationInBox(AABB(boxMin,boxMax));
	return 1;
}

int CPhysStreamer::DestroyPhysicalEntitiesInBox(const Vec3 &boxMin, const Vec3 &boxMax)
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_3DENGINE );
	if (((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree)
	((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree->DephysicalizeVegetationInBox(AABB(boxMin,boxMax));
	return 1;
}